/* 编译检查：<fcntl.h> 与 custom.h 同时包含不得冲突（custom.h 曾包含 <linux/fcntl.h>，会重复定义 struct flock）。 */
#include <fcntl.h>
#include "custom.h"
