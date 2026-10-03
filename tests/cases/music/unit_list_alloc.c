/* 只对音乐源码的 malloc 注入失败，不影响 LVGL 和 cJSON 的分配。 */
#include <stdlib.h>
#include "custom.h"
#include "fake_fs.h"

int tst_music_alloc_fail_at;
int tst_music_alloc_calls;
void *tst_music_malloc(size_t size);
int tst_music_list_empty(void);
#define MUSIC_DIR_PATH tst_music_dir()
#define MPV_SOCKET_PATH tst_mpv_socket_path()
#define malloc tst_music_malloc
#include "custom_musicplayer.c"
#undef malloc

void *tst_music_malloc(size_t size)
{
    tst_music_alloc_calls++;
    if (tst_music_alloc_fail_at == tst_music_alloc_calls)
        return NULL;
    return malloc(size);
}

int tst_music_list_empty(void)
{
    return head == NULL && playing_music_node == NULL;
}
