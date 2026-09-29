#include "capture.h"
#include "fake_fs.h"

#include <fcntl.h>
#include <sanitizer/common_interface_defs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int saved_out = -1;
static int saved_err = -1;
static char *captured;

void tst_capture_begin(void)
{
    char path[512];
    int fd;

    tst_path(path, sizeof(path), "capture.log");
    fflush(stdout);
    fflush(stderr);
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror(path);
        exit(2);
    }
    saved_out = dup(STDOUT_FILENO);
    saved_err = dup(STDERR_FILENO);
    /* sanitizer 报告仍写到原 stderr，否则崩溃时报告会留在被丢弃的捕获文件里。 */
    __sanitizer_set_report_fd((void *)(intptr_t)saved_err);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);
}

const char *tst_capture_end(void)
{
    char path[512];
    FILE *fp;
    long size;

    fflush(stdout);
    fflush(stderr);
    dup2(saved_out, STDOUT_FILENO);
    dup2(saved_err, STDERR_FILENO);
    close(saved_out);
    close(saved_err);
    __sanitizer_set_report_fd((void *)(intptr_t)STDERR_FILENO);

    tst_path(path, sizeof(path), "capture.log");
    fp = fopen(path, "rb");
    if (fp == NULL || fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0) {
        perror(path);
        exit(2);
    }
    rewind(fp);
    free(captured);
    captured = malloc((size_t)size + 1);
    if (captured == NULL || fread(captured, 1, (size_t)size, fp) != (size_t)size) {
        perror(path);
        exit(2);
    }
    captured[size] = '\0';
    fclose(fp);
    printf("---- captured output ----\n%s---- end of captured output ----\n", captured);
    return captured;
}

int tst_count_lines(const char *text, const char *needle)
{
    size_t needle_len = strlen(needle);
    const char *line = text;
    int count = 0;

    while (line != NULL && *line != '\0') {
        const char *end = strchr(line, '\n');
        size_t len = end != NULL ? (size_t)(end - line) : strlen(line);
        const char *p;

        for (p = line; p + needle_len <= line + len; p++) {
            if (memcmp(p, needle, needle_len) == 0) {
                count++;
                break;
            }
        }
        line = end != NULL ? end + 1 : NULL;
    }
    return count;
}
