/* loadfile 命令：含 " 与 255 字节的文件名都按 JSON 正确转义、不溢出（spec §5.4 ③）。 */
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

#include "cJSON.h"
#include "check.h"
#include "music_env.h"

int music_scan_list(void);

static void escaping_and_length(void)
{
    static char buf[65536];
    char longname[256], want_long[300];
    size_t total = 0;
    ssize_t n;
    int peer, found_quote = 0, found_long = 0;
    char *line;

    memset(longname, 'x', 251);
    memcpy(longname + 251, ".mp3", 5);
    snprintf(want_long, sizeof(want_long), "/music/%s", longname);
    peer = music_env_socketpair();
    music_env_file("a\"b.mp3");
    music_env_file(longname);
    music_scan_list();
    while ((n = recv(peer, buf + total, sizeof(buf) - 1 - total, MSG_DONTWAIT)) > 0)
        total += (size_t)n;
    buf[total] = '\0';
    for (line = strtok(buf, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        cJSON *root = cJSON_Parse(line);
        cJSON *cmd = root != NULL ? cJSON_GetObjectItem(root, "command") : NULL;
        cJSON *arg1 = cmd != NULL ? cJSON_GetArrayItem(cmd, 1) : NULL;

        CHECK(root != NULL);
        if (arg1 != NULL && cJSON_IsString(arg1)) {
            found_quote |= strcmp(arg1->valuestring, "/music/a\"b.mp3") == 0;
            found_long |= strcmp(arg1->valuestring, want_long) == 0;
        }
        cJSON_Delete(root);
    }
    CHECK(found_quote);
    CHECK(found_long);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"escaping_and_length", escaping_and_length}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
