/*
* Copyright 2023 NXP
* NXP Confidential and Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/


/*********************
 *      INCLUDES
 *********************/
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "lvgl.h"
#include "custom.h"

/*********************
 *      DEFINES
 *********************/
#define MAX_CONF_LEN 128
#define MAX_LINE_LEN 1024
#define MAX_NETWORKS 10
#ifndef WPA_FILE_PATH
#define WPA_FILE_PATH "/etc/wpa_supplicant.conf"
#endif
/**********************
 *      TYPEDEFS
 **********************/
struct wifi_network {
    char ssid[MAX_CONF_LEN];
};
/**********************
 *  GLOBAL VARIABLES
 **********************/
extern lv_ui guider_ui;
/**********************
 *  STATIC VARIABLES
 **********************/
pthread_t wifi_status_update_thread;
lv_timer_t *wifi_update_timer;
static lv_obj_t *wifi_hint_label;
/**********************
 *  STATIC FUNCTIONS
 **********************/
static int wifi_utf8_valid(const unsigned char *s)
{
    while (*s != '\0') {
        uint32_t cp;
        int n, i;

        if (*s < 0x80) {
            s++;
            continue;
        }
        if ((*s & 0xe0) == 0xc0) {
            n = 1;
            cp = *s & 0x1f;
        } else if ((*s & 0xf0) == 0xe0) {
            n = 2;
            cp = *s & 0x0f;
        } else if ((*s & 0xf8) == 0xf0) {
            n = 3;
            cp = *s & 0x07;
        } else {
            return 0;
        }
        for (i = 1; i <= n; i++) {
            if ((s[i] & 0xc0) != 0x80)
                return 0;
            cp = (cp << 6) | (s[i] & 0x3f);
        }
        if ((n == 1 && cp < 0x80) || (n == 2 && cp < 0x800) || (n == 3 && (cp < 0x10000 || cp > 0x10ffff)) || (cp >= 0xd800 && cp <= 0xdfff))
            return 0;
        s += n + 1;
    }
    return 1;
}

/* 按字节计长度；wpa_supplicant 把引号外的 # 当注释，保守起见值里 # 之前出现过 " 就拒绝。 */
static int wifi_value_ok(const char *value, size_t min_len, size_t max_len)
{
    const unsigned char *p;
    size_t len = strlen(value);
    int seen_quote = 0;

    if (len < min_len || len > max_len)
        return 0;
    for (p = (const unsigned char *)value; *p != '\0'; p++) {
        if (*p < 0x20 || *p == 0x7f)
            return 0;
        if (*p == '"')
            seen_quote = 1;
        else if (*p == '#' && seen_quote)
            return 0;
    }
    return wifi_utf8_valid((const unsigned char *)value);
}

const char *wifi_input_check(const char *ssid, const char *password)
{
    if (!wifi_value_ok(ssid, 1, 32))
        return "Invalid SSID: 1-32 bytes, no control characters, no # after \"";
    if (!wifi_value_ok(password, 8, 63))
        return "Invalid password: 8-63 bytes, no control characters, no # after \"";
    return NULL;
}

static void wifi_hint_show(const char *text)
{
    if (wifi_hint_label == NULL)
        return;
    lv_label_set_text(wifi_hint_label, text);
    lv_obj_clear_flag(wifi_hint_label, LV_OBJ_FLAG_HIDDEN);
}

static void wifi_hint_hide_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (wifi_hint_label != NULL)
        lv_obj_add_flag(wifi_hint_label, LV_OBJ_FLAG_HIDDEN);
}

static const char *wifi_conf_skip_ws(const char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}

/* 去掉行首空白后以 <key>= 开头才算命中，避免 scan_ssid=、bssid=、wpa_psk= 被当成 ssid=、psk=。 */
static int wifi_conf_is_key(const char *line, const char *key)
{
    size_t n = strlen(key);

    line = wifi_conf_skip_ws(line);
    return strncmp(line, key, n) == 0 && line[n] == '=';
}

static int wifi_conf_is_block_start(const char *line)
{
    return strncmp(wifi_conf_skip_ws(line), "network={", 9) == 0;
}

/* 去掉行首空白与行尾 \r、\n、空白后整行为 } 才算块结束，值里的 } 不算。 */
static int wifi_conf_is_block_end(const char *line)
{
    const char *p = wifi_conf_skip_ws(line);

    if (*p != '}')
        return 0;
    for (p++; *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'; p++)
        ;
    return *p == '\0';
}

static void _wifi_conf_load(const char* ssid, const char* password)
{
    FILE *wpa_supplicant_pipe;
    char buffer[MAX_CONF_LEN];

    // hide img
    lv_obj_add_flag(guider_ui.WIFI_wifi_log_img,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(guider_ui.WIFI_loaded_wifi_label,LV_OBJ_FLAG_HIDDEN); 

    // open wpa_supplicant pipe
    wpa_supplicant_pipe = popen("wpa_cli", "w");
    if (wpa_supplicant_pipe == NULL) {
        perror("popen");
        wifi_hint_show("Cannot run wpa_cli");
        return ;
    }
    // set network ssid adn psk
    memset(buffer,0,MAX_CONF_LEN);
    snprintf(buffer, MAX_CONF_LEN, "set_network 0 ssid \"%s\"\n", ssid);
    fputs(buffer, wpa_supplicant_pipe);
    lv_label_set_text(guider_ui.WIFI_loaded_wifi_label,ssid);
    
    memset(buffer,0,MAX_CONF_LEN);
    snprintf(buffer, MAX_CONF_LEN, "set_network 0 psk \"%s\"\n", password);
    fputs(buffer, wpa_supplicant_pipe);

    // save wifi conf
    fputs("save_config\n", wpa_supplicant_pipe);
    pclose(wpa_supplicant_pipe);

    char temp_path[512];
    int n = snprintf(temp_path, sizeof(temp_path), "%s.luckfox.new", WPA_FILE_PATH);
    if (n < 0 || (size_t)n >= sizeof(temp_path)) {
        printf("Config path too long.\n");
        return ;
    }

    // 保存 WiFi 配置到 WPA_FILE_PATH。
    FILE *file = fopen(WPA_FILE_PATH, "r");
    if (file == NULL) {
        printf("Failed to open file.\n");
        return ;
    }

    struct stat st;
    if (fstat(fileno(file), &st) != 0) {
        perror("fstat");
        fclose(file);
        return ;
    }

    FILE *temp_file = fopen(temp_path, "w");
    if (temp_file == NULL) {
        printf("Failed to create temporary file.\n");
        fclose(file);
        return ;
    }

    /* fopen("w") 受 umask 影响，必须成功保留原配置的权限才能替换。 */
    if (fchmod(fileno(temp_file), st.st_mode & 07777) != 0) {
        perror("fchmod");
        fclose(file);
        fclose(temp_file);
        remove(temp_path);
        return ;
    }

    char line[MAX_LINE_LEN];
    int inside_network_block = 0;

    while (fgets(line, MAX_LINE_LEN, file)) {
        // Enter network={} block
        if (wifi_conf_is_block_start(line)) {
            inside_network_block = 1;
            fputs(line, temp_file);
            continue;
        }
        // Exit network={} block
        if (wifi_conf_is_block_end(line)) {
            inside_network_block = 0;
        }
        // Inside network={} block
        if (inside_network_block) {
            if (wifi_conf_is_key(line, "ssid")) {
                memset(buffer,0,MAX_CONF_LEN);
                sprintf(buffer, "        ssid=\"%s\"\n",ssid);
                fputs(buffer, temp_file);
            }
            else if (wifi_conf_is_key(line, "psk")) {
                memset(buffer,0,MAX_CONF_LEN);
                sprintf(buffer, "        psk=\"%s\"\n",password);
                fputs(buffer, temp_file);
            }
            else {
                fputs(line, temp_file);
            }
        }
        else {
            fputs(line, temp_file);
        }
    }

    /* 读写、刷盘和关闭均成功后才允许替换，失败时保留原配置。 */
    int failed = ferror(file) || ferror(temp_file);
    if (fclose(file) != 0)
        failed = 1;
    if (!failed && (fflush(temp_file) != 0 || fsync(fileno(temp_file)) != 0))
        failed = 1;
    if (fclose(temp_file) != 0)
        failed = 1;
    if (failed) {
        printf("Failed to write configuration.\n");
        remove(temp_path);
        return ;
    }
    if (rename(temp_path, WPA_FILE_PATH) != 0) {
        perror("rename");
        remove(temp_path);
        return ;
    }

    // reconnect wifi
    system("wpa_cli reconfigure &");
    system("udhcpc -i wlan0 &");
    return ;
}

/* 取该行第一个到最后一个 " 之间的内容；放不下或没有成对引号时返回 -1，不写 out。 */
static int wifi_conf_quoted(const char *line, char *out, size_t size)
{
    const char *first = strchr(line, '"');
    const char *last = strrchr(line, '"');
    size_t len;

    if (first == NULL || last == first)
        return -1;
    len = (size_t)(last - first - 1);
    if (len >= size)
        return -1;
    memcpy(out, first + 1, len);
    out[len] = '\0';
    return 0;
}

static void _wifi_conf_get(char *ssid, size_t ssid_size, char *passwd, size_t passwd_size)
{
    FILE *file = fopen(WPA_FILE_PATH, "r");
    if (file == NULL) {
        printf("Failed to open file.\n");
        return ;
    }

    char line[MAX_LINE_LEN];
    int inside_network_block = 0;

    while (fgets(line, MAX_LINE_LEN, file)) {
        // Enter network={} block
        if (wifi_conf_is_block_start(line)) {
            inside_network_block = 1;
            continue;
        }
        // Exit network={} block
        if (wifi_conf_is_block_end(line)) {
            inside_network_block = 0;
        }
        // Inside network={} block
        if (inside_network_block) {
            if (wifi_conf_is_key(line, "ssid"))
                wifi_conf_quoted(line, ssid, ssid_size);
            else if (wifi_conf_is_key(line, "psk"))
                wifi_conf_quoted(line, passwd, passwd_size);
        }
    }
    fclose(file);

    return ;
}

static void _wifi_status_update()
{
    FILE *fp;
    char command[MAX_LINE_LEN];
    char result[MAX_LINE_LEN];
    strcpy(command, "wpa_cli status");
 
    fp = popen(command, "r");
    if (fp == NULL) {
        printf("Failed to run command\n");
        return ;
    }


    while (fgets(result, sizeof(result)-1, fp) != NULL)
    {
        if (strstr(result, "wpa_state=COMPLETED"))
        {   
            // connected 
            if((guider_ui.WIFI_loading_spinner != NULL)
                &&(guider_ui.WIFI_wifi_log_img != NULL)
                &&(guider_ui.WIFI_loaded_wifi_label != NULL)  )
            {
                lv_obj_add_flag(guider_ui.WIFI_loading_spinner,LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(guider_ui.WIFI_wifi_log_img,LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(guider_ui.WIFI_loaded_wifi_label,LV_OBJ_FLAG_HIDDEN);

            }
            break;     
        }
        else if(strstr(result, "wpa_state=SCANNING"))
        { 
            // scanning
            if((guider_ui.WIFI_loading_spinner != NULL)
                &&(guider_ui.WIFI_wifi_log_img != NULL)
                &&(guider_ui.WIFI_loaded_wifi_label != NULL)  )
            {
                lv_obj_clear_flag(guider_ui.WIFI_loading_spinner,LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(guider_ui.WIFI_wifi_log_img,LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(guider_ui.WIFI_loaded_wifi_label,LV_OBJ_FLAG_HIDDEN);
            }
            break;     
        }
    }

    pclose(fp);

}

static int wifi_hex(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* hostap printf_encode 的逆过程；空、含控制字符、残缺或未知转义、超过 out_size - 1 字节时返回 -1。 */
static int wifi_ssid_decode(const char *in, char *out, size_t out_size)
{
    size_t n = 0;

    while (*in != '\0') {
        unsigned char c = (unsigned char)*in++;

        if (c == '\\') {
            int hi, lo;

            switch (*in) {
            case '"': c = '"'; in++; break;
            case '\\': c = '\\'; in++; break;
            case 'e': c = 0x1b; in++; break;
            case 'n': c = '\n'; in++; break;
            case 'r': c = '\r'; in++; break;
            case 't': c = '\t'; in++; break;
            case 'x':
                hi = wifi_hex(in[1]);
                lo = hi < 0 ? -1 : wifi_hex(in[2]);
                if (lo < 0)
                    return -1;
                c = (unsigned char)(hi * 16 + lo);
                in += 3;
                break;
            default:
                return -1;
            }
        }
        if (c < 0x20 || c == 0x7f)
            return -1;
        if (n + 1 >= out_size)
            return -1;
        out[n++] = (char)c;
    }
    if (n == 0)
        return -1;
    out[n] = '\0';
    return 0;
}

static int _wifi_scanning_ssid()
{
    struct wifi_network networks[MAX_LINE_LEN];
    int network_count = 0;

    char command[] = "wpa_cli -i wlan0 scan_results";
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        perror("Error opening pipe");
        return -1;
    }

    char line[MAX_LINE_LEN];
    // Skip the first two lines as they contain header information
    fgets(line, MAX_LINE_LEN, fp);

    // scan_results 每行为 bssid\tfreq\tsignal\tflags\tssid；按 TAB 位置取第 5 个字段
    while (fgets(line, MAX_LINE_LEN, fp) != NULL) {
        char *ssid = line;
        int tabs = 0;

        line[strcspn(line, "\r\n")] = '\0';
        while (tabs < 4 && (ssid = strchr(ssid, '\t')) != NULL) {
            ssid++;
            tabs++;
        }
        if (ssid == NULL)
            continue;
        if (wifi_ssid_decode(ssid, networks[network_count].ssid, MAX_CONF_LEN) != 0)
            continue;
        network_count++;
        if (network_count >= MAX_NETWORKS)
            break;
    }

    // 将 SSID 用换行符连接为下拉选项
    char ssid_string[MAX_NETWORKS * MAX_CONF_LEN + sizeof("\n...")];
    size_t used = 0;
    ssid_string[0] = '\0';
    for (int i = 0; i < network_count; i++) {
        /* 始终为末尾的换行与省略号保留四字节，截断项整条撤回。 */
        size_t remaining = sizeof(ssid_string) - used - (sizeof("\n...") - 1);
        int n = snprintf(ssid_string + used, remaining, "%s%s", i ? "\n" : "", networks[i].ssid);
        if (n < 0 || (size_t)n >= remaining) {
            ssid_string[used] = '\0';
            break;
        }
        used += (size_t)n;
    }

    // Print the SSID string
    printf("SSID String:\n%s", ssid_string); 
    if(ssid_string == NULL || *ssid_string == '\0')
    {
        if(guider_ui.WIFI_wifi_list != NULL)
        {
            lv_dropdown_set_options(guider_ui.WIFI_wifi_list, "scanning");
        }
    }
    else
	{
        if(guider_ui.WIFI_wifi_list != NULL)
        {
            strcat(ssid_string, "\n...");
            lv_dropdown_set_options(guider_ui.WIFI_wifi_list, ssid_string);
        }
    }

    pclose(fp);
    return 0;

}

static void wifi_update_timer_cb(lv_timer_t * tmr)
{
    _wifi_status_update();
}

/**********************
 *  GLOBAL FUNCTIONS
 **********************/
void WIFI_load_btn_event_handler(lv_event_t *e)
{ 
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    
    if (code == LV_EVENT_RELEASED)
    {
        const char *ssid = lv_textarea_get_text(guider_ui.WIFI_ssid_ta);
        const char *passwd = lv_textarea_get_text(guider_ui.WIFI_psw_ta);
        const char *err = wifi_input_check(ssid, passwd);

        if (err != NULL) {
            wifi_hint_show(err);
            return;
        }
        _wifi_conf_load(ssid, passwd);
    }
}

void WIFI_clear_btn_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    
    if (code == LV_EVENT_RELEASED)
    {
        lv_textarea_set_text(guider_ui.WIFI_ssid_ta,"");
        lv_textarea_set_text(guider_ui.WIFI_psw_ta,"");
    } 
}

void WIFI_scanning_btn_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    
    if (code == LV_EVENT_RELEASED)
    {
        _wifi_scanning_ssid();
    } 
}

void WIFI_wifi_list_event_handler(lv_event_t *e)
{ 
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        char ssid[MAX_CONF_LEN];
        memset(ssid, 0, MAX_CONF_LEN);
        lv_dropdown_get_selected_str(guider_ui.WIFI_wifi_list, ssid, MAX_CONF_LEN);
        /* 文本框会丢弃非法字节，必须先检查扫描得到的原始 SSID。 */
        if (!wifi_utf8_valid((const unsigned char *)ssid)) {
            wifi_hint_show("Invalid SSID: UTF-8 required");
            return;
        }
        /* 回填前按原始字节长度拒绝，避免文本框截断后提交另一个名称。 */
        if (strlen(ssid) > 32) {
            wifi_hint_show(wifi_input_check(ssid, ""));
            return;
        }
        lv_textarea_set_text(guider_ui.WIFI_ssid_ta, ssid);
    } 
}

void wifi_app_init()
{
    char ssid[MAX_CONF_LEN];
    char passwd[MAX_CONF_LEN];

    memset(ssid, 0, MAX_CONF_LEN);
    memset(passwd, 0, MAX_CONF_LEN);
    _wifi_conf_get(ssid, MAX_CONF_LEN, passwd, MAX_CONF_LEN);


    lv_label_set_text(guider_ui.WIFI_loaded_wifi_label,ssid);
    lv_textarea_set_text(guider_ui.WIFI_ssid_ta, ssid);
    lv_textarea_set_text(guider_ui.WIFI_psw_ta, passwd);

    lv_obj_add_flag(guider_ui.WIFI_wifi_log_img,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(guider_ui.WIFI_loaded_wifi_label,LV_OBJ_FLAG_HIDDEN); 
    wifi_hint_label = lv_label_create(guider_ui.WIFI);
    luckfox_lv_obj_set_pos(wifi_hint_label, 150, 245);
    luckfox_lv_obj_set_size(wifi_hint_label, 310, 60);
    lv_label_set_long_mode(wifi_hint_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(wifi_hint_label, lv_color_hex(0xff0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    luckfox_lv_obj_set_style_text_font(wifi_hint_label, &lv_font_montserratMedium_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(wifi_hint_label, LV_OBJ_FLAG_HIDDEN);
    /* 回填文本框也会发 VALUE_CHANGED，所以在回填之后、标签创建之后才注册。 */
    lv_obj_add_event_cb(guider_ui.WIFI_ssid_ta, wifi_hint_hide_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(guider_ui.WIFI_psw_ta, wifi_hint_hide_cb, LV_EVENT_VALUE_CHANGED, NULL);

}

void wifi_backend_init()
{
    wifi_update_timer = lv_timer_create(wifi_update_timer_cb, 5000, 0); 
    lv_timer_set_repeat_count(wifi_update_timer, -1); 
}

void wifi_backend_release()
{
    if (wifi_update_timer != NULL) {
        lv_timer_del(wifi_update_timer);
        wifi_update_timer = NULL;
    }
}