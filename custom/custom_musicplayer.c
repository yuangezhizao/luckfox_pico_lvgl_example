/*
* Copyright 2023 NXP
* NXP Confidential and Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/


/*********************
 *      INCLUDES
 *********************/
#include <stdio.h>
#include <sys/wait.h>
#include <time.h>
#include "lvgl.h"
#include "custom.h"
/*********************
 *      DEFINES
 *********************/

#define MAX_FILENAME_LEN 256
#define MPV_CONNECT_TIMEOUT_MS 5000
#define MPV_CONNECT_POLL_MS 50
#ifndef MPV_SOCKET_PATH
#define MPV_SOCKET_PATH "/tmp/mpvsocket"
_Static_assert(sizeof(MPV_SOCKET_PATH) <= sizeof(((struct sockaddr_un *)0)->sun_path), "MPV_SOCKET_PATH too long for sun_path");
#endif
#ifndef MUSIC_DIR_PATH
#define MUSIC_DIR_PATH "/music"
#endif
/**********************
 *      TYPEDEFS
 **********************/
struct Music_Node {
    char filename[MAX_FILENAME_LEN];
    int id;
    struct Music_Node* prev;
    struct Music_Node* next;
};

/**********************
 *  STATIC VARIABLES
 **********************/
struct Music_Node *playing_music_node;  
struct Music_Node* head;

int32_t fd_mpv = -1;
struct sigaction act;
pid_t pid;
struct sockaddr_un addr;
pthread_t monitor_thread;

int slider_pressed = 0;
/**********************
 *  GLOBAL VARIABLES
 **********************/
extern lv_ui guider_ui;
extern int MUSIC_ENABLE;
/**********************
 *  STATIC FUNCTIONS
 **********************/
static void sigaction_exit_handler(int sig) { exit(0); }

static void insert_music_node(struct Music_Node** head, char* filename, int id) {
    struct Music_Node* newNode = (struct Music_Node*)malloc(sizeof(struct Music_Node));
    if (newNode == NULL) {
        printf("Memory allocation failed\n");
        exit(1);
    }
    strcpy(newNode->filename, filename);
    newNode->id = id;

    if (*head == NULL) {
        *head = newNode;
        newNode->prev = newNode;
        newNode->next = newNode;
    } else {
        struct Music_Node* tail = (*head)->prev;
        tail->next = newNode;
        newNode->prev = tail;
        newNode->next = *head;
        (*head)->prev = newNode;
    }
}

static char *music_roller_str;

/* 链表是环形的：先断开环再逐个释放；重复扫描不泄漏。 */
static void music_list_clear(void)
{
    struct Music_Node *cur = head;

    if (cur != NULL) {
        cur->prev->next = NULL;
        while (cur != NULL) {
            struct Music_Node *next = cur->next;
            free(cur);
            cur = next;
        }
    }
    head = NULL;
    playing_music_node = NULL;
}

/* 扫描已重建链表；选项分配失败时同时清空节点和选项，避免旧选项指向新歌曲。 */
static int music_build_roller_options(int count)
{
    struct Music_Node *cur = head;
    size_t total = 1, used = 0;
    char *buf;
    int i;

    for (i = 0; i < count; i++, cur = cur->next)
        total += strlen(cur->filename) + 1;
    buf = malloc(total);
    if (buf == NULL) {
        music_list_clear();
        free(music_roller_str);
        music_roller_str = NULL;
        return -1;
    }
    buf[0] = '\0';
    for (i = 0, cur = head; i < count; i++, cur = cur->next)
        used += (size_t)snprintf(buf + used, total - used, "%s%s", i ? "\n" : "", cur->filename);
    free(music_roller_str);
    music_roller_str = buf;
    return 0;
}

const char *music_roller_options(void)
{
    return music_roller_str != NULL ? music_roller_str : "";
}

static size_t utf8_next(const char *s, size_t i, size_t len)
{
    for (i++; i < len && ((unsigned char)s[i] & 0xc0) == 0x80; i++)
        ;
    return i;
}

static size_t utf8_prev(const char *s, size_t i)
{
    for (i--; i > 0 && ((unsigned char)s[i] & 0xc0) == 0x80; i--)
        ;
    return i;
}

static lv_coord_t music_text_width(const char *s, const lv_font_t *font, lv_coord_t letter_space)
{
    return lv_txt_get_width(s, (uint32_t)strlen(s), font, letter_space, LV_TEXT_FLAG_NONE);
}

/* 显示名：整名不超过 max_w 时原样；否则头尾按 UTF-8 字符交替保留，中间用 ...（字体没有 U+2026）。
 * roller 每项是一整行，超宽部分两侧都会被裁掉，前缀相同的长文件名会无法区分。 */
int music_display_name(const char *name, char *out, size_t size, const lv_font_t *font, lv_coord_t letter_space, lv_coord_t max_w)
{
    size_t len = strlen(name), keep_head = 0, keep_tail = len;
    char buf[MAX_FILENAME_LEN + 4];
    int turn = 0, stuck = 0;

    if (size == 0 || len >= MAX_FILENAME_LEN)
        return -1;
    if (music_text_width(name, font, letter_space) <= max_w) {
        snprintf(out, size, "%s", name);
        return 0;
    }
    snprintf(out, size, "...");
    /* 先头后尾交替加一个字符，加不下的一侧停下，两侧都加不下为止。 */
    while (stuck < 2 && keep_head < keep_tail) {
        size_t h = keep_head, t = keep_tail;

        if (turn == 0)
            h = utf8_next(name, keep_head, keep_tail);
        else
            t = utf8_prev(name, keep_tail);
        if (h > t)
            break;
        snprintf(buf, sizeof(buf), "%.*s...%s", (int)h, name, name + t);
        if (music_text_width(buf, font, letter_space) <= max_w && strlen(buf) < size) {
            keep_head = h;
            keep_tail = t;
            memcpy(out, buf, strlen(buf) + 1);
            stuck = 0;
        } else {
            stuck++;
        }
        turn ^= 1;
    }
    return 0;
}

/* 按 roller 选中项的字体与内容宽度设置显示名；下标仍对应链表 id，选中项不变。 */
void music_roller_apply(lv_obj_t *roller)
{
    const lv_font_t *font;
    lv_coord_t letter_space, max_w;
    struct Music_Node *cur = head;
    size_t total = 1, used = 0;
    uint16_t sel;
    char *buf;

    if (roller == NULL || head == NULL)
        return;
    lv_obj_update_layout(roller);
    font = lv_obj_get_style_text_font(roller, LV_PART_SELECTED);
    letter_space = lv_obj_get_style_text_letter_space(roller, LV_PART_SELECTED);
    max_w = lv_obj_get_content_width(roller);
    do {
        total += strlen(cur->filename) + 4;
        cur = cur->next;
    } while (cur != head);
    buf = malloc(total);
    if (buf == NULL)
        return;
    buf[0] = '\0';
    cur = head;
    do {
        char shown[MAX_FILENAME_LEN + 4];

        if (music_display_name(cur->filename, shown, sizeof(shown), font, letter_space, max_w) != 0)
            snprintf(shown, sizeof(shown), "%s", cur->filename);
        used += (size_t)snprintf(buf + used, total - used, "%s%s", cur == head ? "" : "\n", shown);
        cur = cur->next;
    } while (cur != head);
    sel = lv_roller_get_selected(roller);
    lv_roller_set_options(roller, buf, LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(roller, sel, LV_ANIM_OFF);
    free(buf);
}

/* MSG_NOSIGNAL 只作用于本次发送；否决 signal(SIGPIPE, SIG_IGN)（进程级，会影响 WiFi 的 popen 等路径）。 */
/* fd_mpv 的关闭（监听线程发现 mpv 退出时）与发送（UI 线程）在同一把锁内，不会写进已关闭或被复用的 fd。 */
static pthread_mutex_t mpv_fd_lock = PTHREAD_MUTEX_INITIALIZER;

static int mpv_send(const char *buf, size_t len)
{
    ssize_t n;

    pthread_mutex_lock(&mpv_fd_lock);
    if (fd_mpv < 0) {
        pthread_mutex_unlock(&mpv_fd_lock);
        return -1;
    }
    n = send(fd_mpv, buf, len, MSG_NOSIGNAL);
    pthread_mutex_unlock(&mpv_fd_lock);
    if (n != (ssize_t)len) {
        perror("mpv_send");
        return -1;
    }
    return 0;
}

/* 文件名里的 " 与 \ 由 cJSON 转义；路径前缀保持字面量 /music/（板上与 MUSIC_DIR_PATH 相同）。 */
static void music_mpv_loadfile(const char *filename)
{
    char path[sizeof("/music/") + 256];
    cJSON *root = cJSON_CreateObject();
    cJSON *cmd = root != NULL ? cJSON_AddArrayToObject(root, "command") : NULL;
    char *json;

    if (cmd == NULL) {
        cJSON_Delete(root);
        return;
    }
    snprintf(path, sizeof(path), "/music/%s", filename);
    cJSON_AddItemToArray(cmd, cJSON_CreateString("loadfile"));
    cJSON_AddItemToArray(cmd, cJSON_CreateString(path));
    cJSON_AddItemToArray(cmd, cJSON_CreateString("append"));
    json = cJSON_PrintUnformatted(root);
    if (json != NULL) {
        mpv_send(json, strlen(json));
        mpv_send("\n", 1);
        cJSON_free(json);
    }
    cJSON_Delete(root);
}

static void _music_pause(int sta)
{
    char cmd[256];
    sprintf(cmd, "{ \"command\": [\"set_property\", \"pause\",%s] }\n", sta ? "true" : "false");
    //printf("%s\n", cmd);
    mpv_send(cmd, strlen(cmd));
}

static void _music_set_pos(int music_id)
{
    int read_flag = 0;
    char cmd[256];
    sprintf(cmd, "{ \"command\": [\"set_property\", \"playlist-pos\", %d] }\n",music_id);
    //printf("%s\n", cmd);
    mpv_send(cmd, strlen(cmd));
    // set name
    lv_label_set_text(guider_ui.Music_player_music_name, playing_music_node->filename);
}

static void _music_set_volume(int volume)
{
    char cmd[256];
    sprintf(cmd, "{ \"command\": [\"set_property\", \"volume\", %d] }\n",volume);
    //printf("%s\n", cmd);
    mpv_send(cmd, strlen(cmd));
}

static void _music_set_progress(int progress)
{
    char cmd[256];
    sprintf(cmd, "{ \"command\": [\"seek\", %d, \"absolute\"] }\n",progress);
    //printf("%s\n", cmd);
    mpv_send(cmd, strlen(cmd));
}

static void _music_set_mode(int mode)
{
    char cmd[256];
    sprintf(cmd, "{ \"command\": [\"set_property\", \"loop\",%s] }\n", mode ? "true" : "false");
    //printf("%s\n", cmd);
    mpv_send(cmd, strlen(cmd));
}

/* LVGL 不是线程安全的：监听线程只在锁内记录数值与脏标志，由 UI 线程的定时器设置控件。 */
#define MUSIC_UI_PERIOD_MS 200
static pthread_mutex_t music_state_lock = PTHREAD_MUTEX_INITIALIZER;
static int music_time_value, music_duration_value;
static int music_time_dirty, music_duration_dirty;
static lv_timer_t *music_ui_timer;

static void music_ui_timer_cb(lv_timer_t *tmr)
{
    int time_value, duration_value, time_dirty, duration_dirty;
    lv_obj_t *slider = guider_ui.Music_player_progress_slider;

    LV_UNUSED(tmr);
    pthread_mutex_lock(&music_state_lock);
    time_value = music_time_value;
    duration_value = music_duration_value;
    time_dirty = music_time_dirty;
    duration_dirty = music_duration_dirty;
    music_time_dirty = 0;
    music_duration_dirty = 0;
    pthread_mutex_unlock(&music_state_lock);
    if (slider == NULL)
        return;
    if (duration_dirty)
        lv_slider_set_range(slider, 0, duration_value);
    if (time_dirty && !slider_pressed)
        lv_slider_set_value(slider, time_value, LV_ANIM_OFF);
}

/* 解析 mpv 的一行 JSON 事件（监听线程）。 */
static void music_monitor_line(const char *line)
{
    cJSON *root = cJSON_Parse(line);
    cJSON *event, *data, *id;
    int value;

    if (root == NULL)
        return;
    event = cJSON_GetObjectItem(root, "event");
    data = cJSON_GetObjectItem(root, "data");
    id = cJSON_GetObjectItem(root, "id"); //可以用id,也可以用name
    if (cJSON_IsString(event) && strcmp(event->valuestring, "property-change") == 0 && id != NULL && data != NULL)
    {
        /* 与旧实现一致："data": null（属性不可用）按 0 处理。 */
        value = cJSON_IsNumber(data) ? (int)data->valuedouble : 0;
        pthread_mutex_lock(&music_state_lock);
        if (id->valueint == 1)
        {
            music_time_value = value;
            music_time_dirty = 1;
        }
        else if (id->valueint == 2)
        {
            music_duration_value = value;
            music_duration_dirty = 1;
        }
        pthread_mutex_unlock(&music_state_lock);
    }
    cJSON_Delete(root);
}

/* mpv 一次输出多条事件，一行可能被两次 read 拆开：按 \n 累积成整行再解析；
 * 不用 strtok（与 UI 线程日历的 strtok 共用 libc 静态游标）。超过缓冲区的行整行丢弃到下一个 \n。 */
static char monitor_line[2048];
static size_t monitor_len;
static int monitor_overflow;

static void music_monitor_feed(const char *data, size_t n)
{
    while (n > 0) {
        const char *nl = memchr(data, '\n', n);
        size_t seg = nl != NULL ? (size_t)(nl - data) : n;

        if (!monitor_overflow && monitor_len + seg < sizeof(monitor_line)) {
            memcpy(monitor_line + monitor_len, data, seg);
            monitor_len += seg;
        } else {
            monitor_overflow = 1;
        }
        if (nl == NULL)
            return;
        if (!monitor_overflow) {
            monitor_line[monitor_len] = '\0';
            music_monitor_line(monitor_line);
        }
        monitor_len = 0;
        monitor_overflow = 0;
        data = nl + 1;
        n -= seg + 1;
    }
}

void *get_music_playback_time(void *arg)
{
    pthread_detach(pthread_self());
    // get playback-time
    char cmd[] = "{\"command\": [\"observe_property\", 1,\"playback-time\"]}\n ";
 
    // get sum-time 
    char cmd1[] = "{\"command\": [\"observe_property\", 2,\"duration\"]}\n ";
    
    char buf[512];

    mpv_send(cmd, strlen(cmd));
    mpv_send(cmd1, strlen(cmd1));
    monitor_len = 0;
    monitor_overflow = 0;
    while (1)
    {
        ssize_t n = read(fd_mpv, buf, sizeof(buf) - 1);
        if (n > 0)
        {
            buf[n] = '\0';
            music_monitor_feed(buf, (size_t)n);
        }
        else if (n == 0 || (errno != EINTR && errno != EAGAIN))
        {
            break;
        }
        usleep(10000);
    }
    /* mpv 退出（对端关闭）：关闭并置空 fd_mpv，之后的发送直接返回 -1；回收 mpv，不留僵尸（R2）。 */
    printf("mpv connection closed\n");
    pthread_mutex_lock(&mpv_fd_lock);
    close(fd_mpv);
    fd_mpv = -1;
    pthread_mutex_unlock(&mpv_fd_lock);
    if (pid > 0)
        waitpid(pid, NULL, 0);
    return NULL;
}

/**********************
 *  GLOBAL FUNCTIONS
 **********************/

/* 歌曲的筛选规则（扫描与目录比较共用）：普通文件、扩展名 .mp3、名字不含换行
 * （lv_roller 按 \n 计选项，含 \n 的名字会占两项并让下标整体错位）。 */
static int music_is_track(const struct dirent *entry)
{
    const char *ext;

    if (entry->d_type != DT_REG || strchr(entry->d_name, '\n') != NULL)
        return 0;
    ext = strrchr(entry->d_name, '.');
    return ext != NULL && strcmp(ext, ".mp3") == 0;
}

/* 目录中的歌曲名按 readdir 顺序以 \n 连接，格式与 music_roller_options() 相同；目录打不开时为 ""，分配失败返回 NULL。 */
static char *music_dir_signature(void)
{
    DIR *dir = opendir(MUSIC_DIR_PATH);
    struct dirent *entry;
    size_t used = 0, cap = 256;
    char *sig = malloc(cap);

    if (sig == NULL) {
        if (dir != NULL)
            closedir(dir);
        return NULL;
    }
    sig[0] = '\0';
    if (dir == NULL)
        return sig;
    while ((entry = readdir(dir)) != NULL) {
        size_t need;

        if (!music_is_track(entry))
            continue;
        need = used + strlen(entry->d_name) + 2;
        if (need > cap) {
            char *bigger;

            while (cap < need)
                cap *= 2;
            bigger = realloc(sig, cap);
            if (bigger == NULL) {
                free(sig);
                closedir(dir);
                return NULL;
            }
            sig = bigger;
        }
        used += (size_t)snprintf(sig + used, cap - used, "%s%s", used ? "\n" : "", entry->d_name);
    }
    closedir(dir);
    return sig;
}

/* MUSIC 按钮回调先调用：MUSIC_ENABLE 原来只在启动时算一次，运行中放入的音乐要重启才能进入。2（Ubuntu，隐藏音乐）不变。 */
void music_recheck_enable(void)
{
    char *sig;

    if (MUSIC_ENABLE == 2)
        return;
    sig = music_dir_signature();
    if (sig == NULL)
        return;
    MUSIC_ENABLE = sig[0] != '\0';
    free(sig);
}

/* 音乐页只建一次：每次进入时重新扫描，目录有变化才重建列表与 mpv 播放列表，回到第一首并暂停；没变化不打断播放。 */
static void music_refresh_list(void)
{
    char *sig = music_dir_signature();
    static const char stop_cmd[] = "{ \"command\": [\"stop\"] }\n";

    if (sig == NULL)
        return;
    if (strcmp(sig, music_roller_options()) == 0) {
        free(sig);
        return;
    }
    free(sig);
    /* stop 停止播放并清空 mpv 播放列表，随后 music_scan_list() 逐个 loadfile append。 */
    mpv_send(stop_cmd, strlen(stop_cmd));
    music_scan_list();
    lv_roller_set_options(guider_ui.Music_player_roller_1, music_roller_options(), LV_ROLLER_MODE_INFINITE);
    music_roller_apply(guider_ui.Music_player_roller_1);
    pthread_mutex_lock(&music_state_lock);
    music_time_dirty = 0;
    music_duration_dirty = 0;
    pthread_mutex_unlock(&music_state_lock);
    lv_slider_set_value(guider_ui.Music_player_progress_slider, 0, LV_ANIM_OFF);
    lv_obj_clear_state(guider_ui.Music_player_start_btn, LV_STATE_CHECKED);
    if (playing_music_node != NULL) {
        _music_set_pos(0);
        _music_pause(1);
    } else {
        lv_label_set_text(guider_ui.Music_player_music_name, "");
    }
}

static void music_screen_load_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    music_refresh_list();
}

int music_scan_list(void)
{
    DIR *dir;
    struct dirent *entry;
    int id_num = 0;

    music_list_clear();
    dir = opendir(MUSIC_DIR_PATH);
    if (dir == NULL) {
        perror("opendir music dir");
        music_build_roller_options(0);
        return -1;
    }

    // Read files from music dir
    while ((entry = readdir(dir)) != NULL) {
        if (!music_is_track(entry)) {
            if (entry->d_type == DT_REG && strchr(entry->d_name, '\n') != NULL)
                printf("skip music file with newline in name\n");
            continue;
        }
        // insert Music Node
        insert_music_node(&head, entry->d_name,id_num);
        id_num++;
        //add to mpv list
        music_mpv_loadfile(entry->d_name);
    }
    closedir(dir);
    
    playing_music_node = head;
    return music_build_roller_options(id_num);
}

void Music_player_list_roller_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* obj = lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED)
    {
        /* INFINITE 模式下 lv_roller_get_selected() 已对真实选项数取模，可直接对应链表 id。 */
        uint16_t sel = lv_roller_get_selected(obj);
        struct Music_Node *node = head;

        if (node == NULL)
            return;
        do {
            if (node->id == sel) {
                playing_music_node = node;
                _music_set_pos(node->id);
                return;
            }
            node = node->next;
        } while (node != NULL && node != head);
    }
}

void Music_player_next_btn_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_RELEASED)
    {
        if (playing_music_node == NULL)
            return;
        playing_music_node = playing_music_node->next;
        lv_roller_set_selected(guider_ui.Music_player_roller_1,playing_music_node->id,LV_ANIM_OFF);
        // set play music    
        _music_set_pos(playing_music_node->id);
    }
}

void Music_player_pre_btn_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_RELEASED)
    {   
        if (playing_music_node == NULL)
            return;
        playing_music_node = playing_music_node->prev;
        lv_roller_set_selected(guider_ui.Music_player_roller_1,playing_music_node->id,LV_ANIM_OFF);
        // set play music
        _music_set_pos(playing_music_node->id);
    }
}

void Music_player_start_btn_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    
    if (code == LV_EVENT_RELEASED)
    {
        lv_state_t btn_state = lv_obj_get_state(obj);
        if(btn_state &= LV_STATE_CHECKED)
        {
            // Music start
            _music_pause(0);
        }
        else
        {
            // Music stop
            _music_pause(1);
        } 
    }
}

void Music_player_volume_slider_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_RELEASED)
    {
        int volume = lv_slider_get_value(obj);
        _music_set_volume(volume);
    }
}

void Music_player_progress_slider_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_RELEASED)
    { 
        int progress = lv_slider_get_value(obj);
        _music_set_progress(progress);
        slider_pressed = 0;
    }
    else if(code == LV_EVENT_PRESSING)
    {
        slider_pressed = 1;
    }

}

void Music_player_mode_btn_event_handler(lv_event_t *e)
{ 
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_RELEASED)
    { 
        lv_state_t btn_state = lv_obj_get_state(obj);
        if(btn_state &= LV_STATE_CHECKED)
        {
            _music_set_mode(0);
        }
        else
        {
            _music_set_mode(1);
        } 
    }

}

/* 返回已连接的 fd；-2 表示 mpv 已退出且已回收（如 exec 失败的 127）；-1 表示超时或 socket() 失败，子进程仍待回收。 */
static int mpv_wait_connect(pid_t child, const char *sock_path)
{
    struct sockaddr_un sa;
    struct timespec start, now;
    int status;

    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    memcpy(sa.sun_path, sock_path, strlen(sock_path) + 1);
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        int fd;
        long elapsed_ms;

        if (waitpid(child, &status, WNOHANG) == child) {
            printf("mpv exited before its IPC socket appeared (status 0x%x)\n", status);
            return -2;
        }
        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            perror("Create socket failed");
            return -1;
        }
        if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) == 0)
            return fd;
        close(fd);
        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed_ms = (now.tv_sec - start.tv_sec) * 1000L + (now.tv_nsec - start.tv_nsec) / 1000000L;
        if (elapsed_ms >= MPV_CONNECT_TIMEOUT_MS) {
            printf("Cannot connect to %s\n", sock_path);
            return -1;
        }
        usleep(MPV_CONNECT_POLL_MS * 1000);
    }
}

int music_player_thread_init()
{
    /* MPV_SOCKET_PATH 可被测试覆盖为运行期表达式，长度只能在运行期检查，且须在启动 mpv 之前。 */
    const char *sock_path = MPV_SOCKET_PATH;
    char ipc_arg[sizeof("--input-ipc-server=") + sizeof(addr.sun_path)];
    int n;

    if (strlen(sock_path) >= sizeof(addr.sun_path)) {
        printf("mpv socket path too long: %s\n", sock_path);
        return -1;
    }
    n = snprintf(ipc_arg, sizeof(ipc_arg), "--input-ipc-server=%s", sock_path);
    if (n < 0 || (size_t)n >= sizeof(ipc_arg)) {
        printf("mpv ipc argument too long\n");
        return -1;
    }
    /* 移除旧实例的监听路径，已有连接不受影响。 */
    unlink(sock_path);
    pid = vfork();
    if (pid == 0) // child thread
    {
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        execlp("mpv", "mpv", "--quiet", "--no-terminal", "--no-video", "--idle=yes", "--term-status-msg=", ipc_arg, (char *)NULL);
        /* vfork 子进程与父进程共用栈，只能 _exit，不能 return。 */
        _exit(127);
    }
    else if (pid > 0) // parent thread
    {
        int fd;

        act.sa_handler = sigaction_exit_handler;
        sigfillset(&act.sa_mask);
        act.sa_flags = SA_RESTART; /* don't fiddle with EINTR */
        sigaction(SIGUSR1, &act, NULL);

        fd = mpv_wait_connect(pid, sock_path);
        if (fd == -2)
            return -1;
        if (fd < 0) {
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            return -1;
        }
        fd_mpv = fd;
        // Monitor thread
        if (pthread_create(&monitor_thread, NULL, get_music_playback_time, NULL) != 0)
        {
            perror("pthread create error!\n");
            close(fd_mpv);
            fd_mpv = -1;
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            return -1;
        }
        return 0;
    }
    else
    {
        perror("fork error:\n");
        return -1;
    }
}

int music_app_init()
{
    static lv_obj_t *load_cb_screen;

    if (music_ui_timer == NULL)
        music_ui_timer = lv_timer_create(music_ui_timer_cb, MUSIC_UI_PERIOD_MS, NULL);
    if (guider_ui.Music_player != NULL && load_cb_screen != guider_ui.Music_player) {
        lv_obj_add_event_cb(guider_ui.Music_player, music_screen_load_cb, LV_EVENT_SCREEN_LOAD_START, NULL);
        load_cb_screen = guider_ui.Music_player;
    }
    if (playing_music_node == NULL)
        return -1;
    _music_set_pos(0);
    _music_pause(1);
    _music_set_volume(50);
    _music_set_mode(1);
    return 0;
}

int music_app_quit()
{
    _music_pause(1);
    return 0;
}