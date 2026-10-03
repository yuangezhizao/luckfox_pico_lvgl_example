/* luckfox_get_system_info()：popen 失败时按非 Ubuntu 处理且不崩溃（spec §5.1 ⑯）。 */
#include <stdio.h>

#include "check.h"

int luckfox_get_system_info(void);

FILE *popen(const char *command, const char *type)
{
    (void)command;
    (void)type;
    return NULL;
}

static void popen_failure(void)
{
    CHECK_EQ_INT(luckfox_get_system_info(), 0);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"popen_failure", popen_failure}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
