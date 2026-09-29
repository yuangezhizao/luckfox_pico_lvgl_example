luckfox_add_test(sdio_detect UNIT unit_sdio_detect.c
    CASES absent empty func1_only ultra_w hidden no_newline prefix)
luckfox_add_test(brightness
    CASES drag_writes_min_and_max small_max_never_writes_zero_9 small_max_never_writes_zero_1
          initial_zero_not_written popup_blocks_drag
          hidden_no_device hidden_non_numeric_max hidden_zero_max hidden_oversized_max
          write_failure_reported_once writes_deduplicated)
