/* 未指定参数的旧声明可兼容带 int 参数的函数，完整的 void 原型必须拒绝这种兼容。 */
#include "custom.h"

_Static_assert(!__builtin_types_compatible_p(__typeof__(&wifi_app_init), void (*)(int)), "wifi_app_init 必须声明 void 参数");
_Static_assert(!__builtin_types_compatible_p(__typeof__(&wifi_backend_init), void (*)(int)), "wifi_backend_init 必须声明 void 参数");
_Static_assert(!__builtin_types_compatible_p(__typeof__(&wifi_backend_release), void (*)(int)), "wifi_backend_release 必须声明 void 参数");
_Static_assert(__builtin_types_compatible_p(__typeof__(&wifi_input_check), const char *(*)(const char *, const char *)), "wifi_input_check 必须有完整原型");
