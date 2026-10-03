luckfox_add_test(fb_clear CASES enospc_silent cap open_failure zero_write_silent eintr_retries)
target_link_options(exit_fb_clear PRIVATE -Wl,--wrap=write)
