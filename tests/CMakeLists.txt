cmake_minimum_required(VERSION 3.16)
project(luckfox_lvgl_tests C)

# 主机原生测试工程，与顶层交叉编译工程互不引用；用法见 README.md。
set(LUCKFOX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "被测源码树根目录（tools/mutate.sh 指向改坏副本）")
set(LUCKFOX_TESTS_ARM32 AUTO CACHE STRING "32 位测试：AUTO 找到工具才注册，ON 缺工具即报错，OFF 不注册")
set_property(CACHE LUCKFOX_TESTS_ARM32 PROPERTY STRINGS AUTO ON OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS ON)
set(LUCKFOX_SANITIZE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
add_compile_options(-g -O1 ${LUCKFOX_SANITIZE})
add_link_options(${LUCKFOX_SANITIZE})

enable_testing()

set(LUCKFOX_INCLUDE_DIRS
    ${LUCKFOX_ROOT}/src
    ${LUCKFOX_ROOT}/include
    /usr/include/libdrm
    /usr/include/cjson
    ${LUCKFOX_ROOT}/lib
    ${LUCKFOX_ROOT}/lib/lvgl
    ${LUCKFOX_ROOT}/lib/lvgl/src
    ${LUCKFOX_ROOT}/custom
    ${LUCKFOX_ROOT}/generated
    ${LUCKFOX_ROOT}/generated/guider_customer_fonts
    ${LUCKFOX_ROOT}/generated/guider_fonts
    ${LUCKFOX_ROOT}/generated/images
)

# 被测源码：告警基线由交叉编译记录（spec §5.1），此处以 -w 编译。
file(GLOB_RECURSE LUCKFOX_APP_SOURCES CONFIGURE_DEPENDS
    ${LUCKFOX_ROOT}/lib/lvgl/src/*.c
    ${LUCKFOX_ROOT}/generated/*.c
    ${LUCKFOX_ROOT}/custom/*.c
)
add_library(luckfox_app STATIC ${LUCKFOX_APP_SOURCES})
target_include_directories(luckfox_app SYSTEM PUBLIC ${LUCKFOX_INCLUDE_DIRS})
target_compile_options(luckfox_app PRIVATE -w)
target_link_libraries(luckfox_app PUBLIC drm cjson pthread m)
# 背光目录指向本进程临时目录（spec D6）。
set_source_files_properties(${LUCKFOX_ROOT}/custom/custom_brightness.c PROPERTIES
    COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/support/fake_fs.h"
    COMPILE_DEFINITIONS "BACKLIGHT_SYSFS_DIR=tst_backlight_root()")

# OBJECT 库：被测源码引用这里定义的全局量，这里又引用被测源码，两个静态库互相引用会受链接顺序影响。
add_library(tst_support OBJECT
    support/app_env.c
    support/capture.c
    support/fake_fs.c
)
target_include_directories(tst_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(tst_support PUBLIC luckfox_app)
target_compile_options(tst_support PRIVATE -Wall -Wextra -Werror)

# luckfox_add_test(<对象> [UNIT <文件>] [CASES <用例>...] [LABELS <标签>...])
# 类别取所在目录名；可执行 <类别>_<对象>；测试名 <类别>.<对象>.<用例>，无 CASES 时为 <类别>.<对象>。
function(luckfox_add_test name)
    cmake_parse_arguments(T "" "UNIT" "CASES;LABELS" ${ARGN})
    get_filename_component(category "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
    set(exe ${category}_${name})
    add_executable(${exe} test_${name}.c)
    target_compile_options(${exe} PRIVATE -Wall -Wextra -Werror)
    if(T_UNIT)
        add_library(${exe}_unit OBJECT ${T_UNIT})
        target_link_libraries(${exe}_unit PRIVATE luckfox_app)
        target_compile_options(${exe}_unit PRIVATE -w)
        target_link_libraries(${exe} PRIVATE ${exe}_unit)
    endif()
    target_link_libraries(${exe} PRIVATE tst_support luckfox_app)
    set(tests "")
    if(T_CASES)
        foreach(c IN LISTS T_CASES)
            add_test(NAME ${category}.${name}.${c} COMMAND ${exe} ${c})
            list(APPEND tests ${category}.${name}.${c})
        endforeach()
    else()
        add_test(NAME ${category}.${name} COMMAND ${exe})
        list(APPEND tests ${category}.${name})
    endif()
    set(labels ${category} ${T_LABELS})
    set_tests_properties(${tests} PROPERTIES TIMEOUT 120 LABELS "${labels}")
endfunction()

add_subdirectory(cases/main)
