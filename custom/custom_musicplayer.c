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

/* MSG_NOSIGNAL 只作用于本次发送；否决 signal(SIGPIPE, SIG_IGN)（进程级，会影响 WiFi 的 popen 等路径）。 */
static int mpv_send(const char *buf, size_t len)
{
    if (fd_mpv < 0)
        return -1;
    ssize_t n = send(fd_mpv, buf, len, MSG_NOSIGNAL);

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

void *get_music_playback_time(void *arg)
{
    pthread_detach(pthread_self());
    // get playback-time
    char cmd[] = "{\"command\": [\"observe_property\", 1,\"playback-time\"]}\n ";
 
    // get sum-time 
    char cmd1[] = "{\"command\": [\"observe_property\", 2,\"duration\"]}\n ";
    
    cJSON *root;
    cJSON *event;
    cJSON *cjson_obj;
    char buf[512];

    mpv_send(cmd, strlen(cmd));
    mpv_send(cmd1, strlen(cmd1));
    while (1)
    {
        memset(buf, 0, sizeof(buf));
        ssize_t n = read(fd_mpv, buf, sizeof(buf) - 1);
        if (n > 0)
        {
            buf[n] = '\0';
            // printf("%s len:%d", buf, strlen(buf));
            // Get one line data
			char *temp = strtok(buf, "\n"); 
            while (temp)
            {
                root = cJSON_Parse(temp);
                if (root != NULL)
                {
                    if (cJSON_HasObjectItem(root, "event"))
                    {
                        event = cJSON_GetObjectItem(root, "event");
                        if (event != NULL)
                        {
                            if (strcmp(event->valuestring, "property-change") == 0)
                            {
                                cjson_obj = cJSON_GetObjectItem(root, "data");
                                cJSON *id = cJSON_GetObjectItem(root, "id"); //可以用id,也可以用name
                                if ((id != NULL) && (cjson_obj != NULL))
                                {
                                    if (id->valueint == 1)
                                    {
                                        if(!slider_pressed)
                                            lv_slider_set_value(guider_ui.Music_player_progress_slider, (int)cjson_obj->valuedouble,LV_ANIM_OFF);
                                    }
                                    else if (id->valueint == 2) 
                                    {
                                        lv_slider_set_range(guider_ui.Music_player_progress_slider, 0, (int)cjson_obj->valuedouble);
                                    }
                                }
                            }
                        }
                    }
                    cJSON_Delete(root);
                }

                temp = strtok(NULL, "\n");
            }
        }
        usleep(10000);
    }
    pthread_exit(NULL);
}

/**********************
 *  GLOBAL FUNCTIONS
 **********************/

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
        if (entry->d_type == DT_REG) { // common file
            /* lv_roller 按 \n 计选项，含 \n 的名字会占两项并让下标整体错位。 */
            if (strchr(entry->d_name, '\n') != NULL) {
                printf("skip music file with newline in name\n");
                continue;
            }
            // check .mp3
            char *ext = strrchr(entry->d_name, '.');
            if (ext != NULL && strcmp(ext, ".mp3") == 0) {
                // insert Music Node
                insert_music_node(&head, entry->d_name,id_num);
                id_num++;
                //add to mpv list            
                music_mpv_loadfile(entry->d_name);
            }
        }
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