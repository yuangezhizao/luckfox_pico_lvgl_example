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
        exit(EXIT_FAILURE);
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
        if (strstr(line, "network={")) {
            inside_network_block = 1;
            fputs(line, temp_file);
            continue;
        }
        // Exit network={} block
        if (strstr(line, "}")) {
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

static void _wifi_conf_get(char* ssid, char* passwd)
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
        if (strstr(line, "network={")) {
            inside_network_block = 1;
            continue;
        }
        // Exit network={} block
        if (strstr(line, "}")) {
            inside_network_block = 0;
        }
        // Inside network={} block
        if (inside_network_block) {
            if (wifi_conf_is_key(line, "ssid")) {
                sscanf(line, " ssid=\"%[^\"]\"", ssid);
            }
            if (wifi_conf_is_key(line, "psk")) {
                sscanf(line, " psk=\"%[^\"]\"", passwd);
            }
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
        pclose(fp);
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

    // Parse each line to extract the SSID
    while (fgets(line, MAX_LINE_LEN, fp) != NULL) {
        char *token = strtok(line, "\t ");
        int count = 0;
        while (token != NULL) {
            if (count == 4) {
                strncpy(networks[network_count].ssid, token, MAX_CONF_LEN);
                networks[network_count].ssid[MAX_CONF_LEN - 1] = '\0'; // Ensure null-termination
                if (strlen(networks[network_count].ssid) > 0) {
                    network_count++;
                }
                break;
            }
            token = strtok(NULL, "\t ");
            count++;
        }
        if (network_count >= MAX_NETWORKS)
            break;
    }

    // Create a string with SSIDs separated by '\n'
    char ssid_string[MAX_NETWORKS * (MAX_CONF_LEN + 1)]; // 1 additional character for '\n'
    ssid_string[0] = '\0'; // Ensure ssid_string is empty initially

    for (int i = 0; i < network_count; i++) {
        if(strcmp(networks[i].ssid,"\n") && strlen(networks[i].ssid) < 16 )
        {
            strcat(ssid_string, networks[i].ssid);
        }
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
    _wifi_conf_get(ssid,passwd);


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