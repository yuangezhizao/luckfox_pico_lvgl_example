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

# 32 位测试：ARM 交叉 gcc 编译、qemu-arm 运行（spec §5.3）。
set(LUCKFOX_ARM32_ENABLED OFF)
if(NOT LUCKFOX_TESTS_ARM32 STREQUAL "OFF")
    find_program(LUCKFOX_ARM32_CC arm-linux-gnueabihf-gcc)
    find_program(LUCKFOX_QEMU_ARM qemu-arm)
    if(EXISTS "${LUCKFOX_ARM32_CC}" AND EXISTS "${LUCKFOX_QEMU_ARM}")
        set(LUCKFOX_ARM32_ENABLED ON)
    elseif(LUCKFOX_TESTS_ARM32 STREQUAL "ON")
        message(FATAL_ERROR "LUCKFOX_TESTS_ARM32=ON，但未找到 arm-linux-gnueabihf-gcc 或 qemu-arm（sudo apt-get install -y qemu-user）")
    else()
        message(STATUS "未找到 arm-linux-gnueabihf-gcc 或 qemu-arm，跳过 32 位测试")
    endif()
endif()
set(LUCKFOX_ARM32_INCLUDE_FLAGS
    -I${LUCKFOX_ROOT}/src -I${LUCKFOX_ROOT}/include
    -I${LUCKFOX_ROOT}/include/glibc/drm -I${LUCKFOX_ROOT}/include/glibc/drm/libdrm -I${LUCKFOX_ROOT}/include/glibc/cjson
    -I${LUCKFOX_ROOT}/lib -I${LUCKFOX_ROOT}/lib/lvgl -I${LUCKFOX_ROOT}/lib/lvgl/src
    -I${LUCKFOX_ROOT}/custom -I${LUCKFOX_ROOT}/generated -I${LUCKFOX_ROOT}/generated/guider_customer_fonts
    -I${LUCKFOX_ROOT}/generated/guider_fonts -I${LUCKFOX_ROOT}/generated/images)

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

# WiFi 配置文件指向本进程临时目录（spec D14）。
set_source_files_properties(${LUCKFOX_ROOT}/custom/custom_wifi.c PROPERTIES
    COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/support/fake_fs.h"
    COMPILE_DEFINITIONS "WPA_FILE_PATH=tst_wpa_conf_path()")

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
        target_include_directories(${exe}_unit PRIVATE ${PROJECT_SOURCE_DIR}/support)
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

# luckfox_add_compile_check(<用例> FLAGS <选项>... SOURCES <文件>...)：以普通 -I 编译每个源文件，任一失败即测试失败；测试名 <类别>.compile.<用例>。
# 不继承 luckfox_app 的 SYSTEM 包含目录：-isystem 下 GCC 不报系统头里展开的宏的诊断，lib/lv_conf.h 里的 custom_tick_get 会漏检。
function(luckfox_add_compile_check name)
    cmake_parse_arguments(C "" "" "FLAGS;SOURCES" ${ARGN})
    get_filename_component(category "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
    set(flags -std=gnu99)
    foreach(dir IN LISTS LUCKFOX_INCLUDE_DIRS)
        list(APPEND flags "-I${dir}")
    endforeach()
    list(APPEND flags ${C_FLAGS})
    string(REPLACE ";" "|" flags "${flags}")
    string(REPLACE ";" "|" sources "${C_SOURCES}")
    add_test(NAME ${category}.compile.${name}
        COMMAND ${CMAKE_COMMAND} -DCC=${CMAKE_C_COMPILER} -DFLAGS=${flags} -DSOURCES=${sources} -P ${PROJECT_SOURCE_DIR}/tools/compile_check.cmake)
    set_tests_properties(${category}.compile.${name} PROPERTIES TIMEOUT 120 LABELS "${category};compile")
endfunction()

add_subdirectory(cases/main)
add_subdirectory(cases/wifi)
# 音乐目录指向本进程临时目录（spec §5.4）。
set_source_files_properties(${LUCKFOX_ROOT}/custom/custom_musicplayer.c PROPERTIES
    COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/support/fake_fs.h"
    COMPILE_DEFINITIONS "MUSIC_DIR_PATH=tst_music_dir();MPV_SOCKET_PATH=tst_mpv_socket_path()")

add_subdirectory(cases/sketchpad)
add_subdirectory(cases/music)

# 手动目标，不进 ALL、不注册为 ctest（spec §5.6）。
add_executable(luckfox_screenshots EXCLUDE_FROM_ALL tools/screenshots.c)
target_compile_options(luckfox_screenshots PRIVATE -Wall -Wextra -Werror)
target_link_libraries(luckfox_screenshots PRIVATE tst_support luckfox_app)
set(shot_dir ${CMAKE_BINARY_DIR}/screenshots)
set(shot_cmds COMMAND ${CMAKE_COMMAND} -E make_directory ${shot_dir})
find_program(LUCKFOX_FFMPEG ffmpeg)
foreach(res 480 720)
    foreach(scenario drag popup no-device)
        list(APPEND shot_cmds COMMAND $<TARGET_FILE:luckfox_screenshots> ${res} ${scenario} ${shot_dir}/${res})
    endforeach()
    if(LUCKFOX_FFMPEG)
        foreach(name initial left popup no-device)
            list(APPEND shot_cmds COMMAND ${LUCKFOX_FFMPEG} -nostdin -loglevel error -y -i ${shot_dir}/${res}-${name}.ppm ${shot_dir}/${res}-${name}.png)
        endforeach()
    endif()
endforeach()
add_custom_target(screenshots ${shot_cmds} VERBATIM)
add_dependencies(screenshots luckfox_screenshots)

# SDL2 先按 config 模式找，找不到退回 pkg-config（CMake 没有 FindSDL2 模块）。
find_package(SDL2 CONFIG QUIET)
if(TARGET SDL2::SDL2)
    set(LUCKFOX_SDL2_LINK SDL2::SDL2)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
        if(SDL2_FOUND)
            set(LUCKFOX_SDL2_LINK PkgConfig::SDL2)
        endif()
    endif()
endif()
if(LUCKFOX_SDL2_LINK)
    add_executable(preview EXCLUDE_FROM_ALL tools/preview.c)
    target_compile_options(preview PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(preview PRIVATE tst_support luckfox_app ${LUCKFOX_SDL2_LINK})
else()
    message(STATUS "未找到 SDL2，不生成 preview 目标")
endif()
