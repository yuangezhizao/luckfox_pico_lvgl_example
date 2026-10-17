#define _XOPEN_SOURCE 700

#include "fake_fs.h"

#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

unsigned long long tst_fb_clear_max_bytes = 64ULL << 20;

static char tmp_dir[256];
static char backlight_root[320];

static void die(const char *what)
{
    perror(what);
    exit(2);
}

static int remove_entry(const char *path, const struct stat *sb, int flag, struct FTW *ftwbuf)
{
    (void)sb;
    (void)flag;
    (void)ftwbuf;
    return remove(path);
}

static void remove_tmp_dir(void)
{
    nftw(tmp_dir, remove_entry, 16, FTW_DEPTH | FTW_PHYS);
}

void tst_fmt(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= size) {
        fprintf(stderr, "tst_fmt: output truncated (format \"%s\")\n", fmt);
        exit(2);
    }
}

const char *tst_tmp_dir(void)
{
    if (tmp_dir[0] == '\0') {
        const char *base = getenv("TMPDIR");

        tst_fmt(tmp_dir, sizeof(tmp_dir), "%s/luckfox-tests-XXXXXX", base != NULL && base[0] != '\0' ? base : "/tmp");
        if (mkdtemp(tmp_dir) == NULL)
            die("mkdtemp");
        atexit(remove_tmp_dir);
    }
    return tmp_dir;
}

const char *tst_backlight_root(void)
{
    if (backlight_root[0] == '\0')
        tst_path(backlight_root, sizeof(backlight_root), "sys/class/backlight");
    return backlight_root;
}

void tst_path(char *buf, size_t size, const char *rel)
{
    tst_fmt(buf, size, "%s/%s", tst_tmp_dir(), rel);
}

void tst_mkdirs(const char *rel)
{
    char path[512];
    char *p;

    tst_path(path, sizeof(path), rel);
    for (p = path + strlen(tst_tmp_dir()) + 1; ; p++) {
        if (*p == '/' || *p == '\0') {
            char saved = *p;

            *p = '\0';
            if (mkdir(path, 0755) != 0 && errno != EEXIST)
                die(path);
            if (saved == '\0')
                break;
            *p = saved;
        }
    }
}

void tst_write_file(const char *rel, const char *content)
{
    char path[512];
    FILE *fp;

    tst_path(path, sizeof(path), rel);
    fp = fopen(path, "w");
    if (fp == NULL)
        die(path);
    if (fputs(content, fp) == EOF || fclose(fp) != 0)
        die(path);
}

long tst_read_long(const char *rel)
{
    char path[512];
    long value = -1;
    FILE *fp;

    tst_path(path, sizeof(path), rel);
    fp = fopen(path, "r");
    if (fp == NULL)
        return -1;
    if (fscanf(fp, "%ld", &value) != 1)
        value = -1;
    fclose(fp);
    return value;
}

const char *tst_wpa_conf_path(void)
{
    static char path[1024];

    if (path[0] == '\0')
        tst_path(path, sizeof(path), "wpa_supplicant.conf");
    return path;
}

/* 在 <临时目录>/bin 写一个可执行脚本；首次调用时把该目录放到 PATH 最前。 */
void tst_fake_exec(const char *name, const char *script)
{
    static int path_set;
    char rel[256], path[1024];

    tst_mkdirs("bin");
    tst_fmt(rel, sizeof(rel), "bin/%s", name);
    tst_write_file(rel, script);
    tst_path(path, sizeof(path), rel);
    if (chmod(path, 0755) != 0) {
        perror("chmod");
        abort();
    }
    if (!path_set) {
        char bin[1024], value[4096];
        const char *old = getenv("PATH");

        tst_path(bin, sizeof(bin), "bin");
        tst_fmt(value, sizeof(value), "%s:%s", bin, old != NULL ? old : "/usr/bin:/bin");
        setenv("PATH", value, 1);
        path_set = 1;
    }
}

const char *tst_music_dir(void)
{
    static char path[1024];
    if (path[0] == '\0')
        tst_path(path, sizeof(path), "music");
    return path;
}

const char *tst_mpv_socket_path(void)
{
    static char path[1024];
    if (path[0] == '\0')
        tst_path(path, sizeof(path), "mpv.sock");
    return path;
}

/* readdir 的顺序由文件系统决定：ext4（dir_index）按文件名哈希加 mkfs 时的随机种子排列，tmpfs 随内核版本按创建顺序或其逆序。
 * 音乐页按读取顺序插入有序链表，若读到的恰为升序（或降序），不排序的改坏也能得到正确结果，测试是否有效取决于所在机器
 * （CI 变体检验 spec §2.4、Q12）。这里先读出全部真实条目，按文件名排序后以固定的非单调顺序返回：「.」「..」在前，
 * 其余把后半与前半交错，如 01 02 03 04 → 03 01 04 02，「总是追加」与「总是插到开头」都排不出升序。 */
#define TST_DIR_SLOTS 4

static struct {
    DIR *dir;
    struct dirent *ents;
    size_t n, pos;
} dir_slots[TST_DIR_SLOTS];

static int dirent_name_cmp(const void *a, const void *b)
{
    return strcmp(((const struct dirent *)a)->d_name, ((const struct dirent *)b)->d_name);
}

static int dirent_is_dot(const struct dirent *e)
{
    return strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0;
}

static int dir_slot(DIR *dir)
{
    struct dirent *all = NULL, *out, *e;
    size_t n = 0, cap = 0, dots = 0, m, h, i;
    int s, free_slot = -1;

    for (s = 0; s < TST_DIR_SLOTS; s++) {
        if (dir_slots[s].dir == dir)
            return s;
        if (dir_slots[s].dir == NULL && free_slot < 0)
            free_slot = s;
    }
    if (free_slot < 0) {
        fprintf(stderr, "tst_readdir: more than %d directories open\n", TST_DIR_SLOTS);
        exit(2);
    }
    while ((e = readdir(dir)) != NULL) {
        if (n == cap) {
            cap = cap ? cap * 2 : 16;
            all = realloc(all, cap * sizeof(*all));
            if (all == NULL)
                die("realloc");
        }
        all[n++] = *e;
    }
    qsort(all, n, sizeof(*all), dirent_name_cmp);
    out = malloc((n ? n : 1) * sizeof(*out));
    if (out == NULL)
        die("malloc");
    for (i = 0; i < n; i++)
        if (dirent_is_dot(&all[i]))
            out[dots++] = all[i];
    m = 0;
    for (i = 0; i < n; i++)
        if (!dirent_is_dot(&all[i]))
            all[m++] = all[i];
    h = m / 2;
    for (i = 0; i < m; i++)
        out[dots + i] = (i % 2 == 0) ? all[h + i / 2] : all[i / 2];
    free(all);
    dir_slots[free_slot].dir = dir;
    dir_slots[free_slot].ents = out;
    dir_slots[free_slot].n = n;
    dir_slots[free_slot].pos = 0;
    return free_slot;
}

struct dirent *tst_readdir(DIR *dir)
{
    int s = dir_slot(dir);

    if (dir_slots[s].pos < dir_slots[s].n)
        return &dir_slots[s].ents[dir_slots[s].pos++];
    return NULL;
}

int tst_closedir(DIR *dir)
{
    int s;

    for (s = 0; s < TST_DIR_SLOTS; s++) {
        if (dir_slots[s].dir == dir) {
            free(dir_slots[s].ents);
            dir_slots[s].dir = NULL;
            dir_slots[s].ents = NULL;
        }
    }
    return closedir(dir);
}
