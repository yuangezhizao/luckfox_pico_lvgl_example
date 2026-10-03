#include <stdbool.h>
#include <stddef.h>

bool tst_canvas_alloc_fail;
void *tst_canvas_alloc(size_t size);
#define SKETCHPAD_BUF_ALLOC(size) tst_canvas_alloc(size)
#include "custom_sketchpad.c"

void *tst_canvas_alloc(size_t size)
{
    return tst_canvas_alloc_fail ? NULL : lv_mem_alloc(size);
}
