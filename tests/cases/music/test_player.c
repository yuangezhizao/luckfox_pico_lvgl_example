/* music_player_thread_init()：mpv 启动、连接、超时与回收（spec D7）。 */
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "check.h"
#include "music_env.h"

int music_player_thread_init(void);

/* PATH 只含本进程的 bin 目录：其中没有 mpv 时 execlp 必然失败。 */
static void path_only_bin(void)
{
    char bin[1024];

    tst_mkdirs("bin");
    tst_path(bin, sizeof(bin), "bin");
    setenv("PATH", bin, 1);
}

static void exec_failure(void)
{
    path_only_bin();
    CHECK_EQ_INT(music_player_thread_init(), -1);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"exec_failure", exec_failure}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
