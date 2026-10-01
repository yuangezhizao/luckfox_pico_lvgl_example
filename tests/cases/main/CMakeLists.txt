luckfox_add_test(sdio_detect UNIT unit_sdio_detect.c
    CASES absent empty func1_only ultra_w hidden no_newline prefix)
luckfox_add_test(brightness
    CASES drag_writes_min_and_max small_max_never_writes_zero_9 small_max_never_writes_zero_1
          initial_zero_not_written popup_blocks_drag
          hidden_no_device hidden_non_numeric_max hidden_zero_max hidden_oversized_max
          write_failure_reported_once writes_deduplicated)
if(LUCKFOX_ARM32_ENABLED)
    set(arm32_dir ${CMAKE_CURRENT_BINARY_DIR}/arm32)
    set(arm32_exe ${arm32_dir}/main_brightness_arm32)
    # -ffunction-sections 与 --gc-sections 丢弃未引用的 brightness_ui_create() 及其 LVGL 依赖。
    add_custom_command(
        OUTPUT ${arm32_exe}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${arm32_dir}
        COMMAND ${LUCKFOX_ARM32_CC} -std=gnu99 -O1 -w -ffunction-sections -fdata-sections ${LUCKFOX_ARM32_INCLUDE_FLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/unit_brightness_arm32.c -o ${arm32_dir}/unit.o
        COMMAND ${LUCKFOX_ARM32_CC} -std=gnu99 -O1 -Wall -Wextra -Werror -I${PROJECT_SOURCE_DIR}/support -c ${CMAKE_CURRENT_SOURCE_DIR}/test_brightness_arm32.c -o ${arm32_dir}/test.o
        COMMAND ${LUCKFOX_ARM32_CC} -Wl,--gc-sections ${arm32_dir}/unit.o ${arm32_dir}/test.o -o ${arm32_exe}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/unit_brightness_arm32.c ${CMAKE_CURRENT_SOURCE_DIR}/test_brightness_arm32.c
                ${LUCKFOX_ROOT}/custom/custom_brightness.c ${PROJECT_SOURCE_DIR}/support/check.h
        VERBATIM)
    add_custom_target(main_brightness_arm32 ALL DEPENDS ${arm32_exe})
    add_test(NAME main.brightness_arm32.conversion COMMAND ${LUCKFOX_QEMU_ARM} -L /usr/arm-linux-gnueabihf ${arm32_exe})
    set_tests_properties(main.brightness_arm32.conversion PROPERTIES TIMEOUT 120 LABELS "main;arm32")
endif()
