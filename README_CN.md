![luckfox](https://github.com/LuckfoxTECH/luckfox-pico/assets/144299491/cec5c4a5-22b9-4a9a-abb1-704b11651e88)
# Luckfox Pico LVGL example
[English](./README.md)
+ 本例程基于 GUI-Guider、lvgl-8.3 设计界面
+ 本例程基于 lv_driver-8.1 驱动显示设备和输入设备
+ 专为`Luckfox Pico Ultra`系列开发板提供的图形界面开发例程

## 支持平台
|型号|操作系统|LF40-720720-ARK|LF40-480480-ARK|
|-----------------------|---------|----|----|
|Luckfox Pico Ultra     |Buildroot/Ubuntu|支持|支持|
|Luckfox Pico Ultra W   |BUildroot/Ubuntu|支持|支持|

## 实现效果
![LVGL_main](images/LVGL_main.png)

**Ubuntu 系统目前仅支持PAD和GIF界面**
+ **WIFI**：配置 Wi-Fi 的SSID和密码，在 Luckfox Pico Ultra 上不会显示该按键。按 **Scan** 会请求 `wpa_supplicant` 扫描，约 3 秒后列出结果
    + 前提：`/etc/wpa_supplicant.conf` 须按 [Luckfox Wi-Fi 文档](https://wiki.luckfox.com/zh/Luckfox-Pico-Ultra/WiFi-BT/) 的模板编写，包含控制接口配置和一个 `network={}` 块。开机时 `rkwifi_server` 把它复制为 `/data/wpa_supplicant.conf` 并以该副本启动 `wpa_supplicant`，修改后需重启
        ```
        ctrl_interface=/var/run/wpa_supplicant
        ap_scan=1
        update_config=1

        network={
                ssid="luckfox"
                psk="12345678"
                key_mgmt=WPA-PSK
        }
        ```
    + 按 **Load** 会把这个 `network={}` 块里的 `ssid`、`psk` 替换为页面上输入的值，上面的占位值只是起点。没有这个块时页面提示 `No network block in wpa_supplicant.conf`；文件为空时 `wpa_cli` 连不上 `wpa_supplicant`，页面提示 `wpa_supplicant not reachable`
+ **PAD**：触摸屏手写测试。轻点会留下一个点；笔迹只跟随第一根按下的手指，其他手指或手掌的接触会被忽略
+ **MUSIC**：音乐播放，仅支持 `.mp3` 格式，需要将播放的音乐文件放置到 `/music` 中。运行中放入或删除的文件，下次进入 MUSIC 页面时生效
+ **GIF**：动图帧率测试
+ **OFF**：关闭程序

## Buildroot
### 编译
+ 设置环境变量
    ```
    export LUCKFOX_SDK_PATH=< luckfox-pico Sdk 地址 >
    ```
    **注意**：使用绝对地址。
+ 使用 CMake 编译获取可执行程序 
    ```
    mkdir build
    cd build
    cmake ..
    make -j
    ```

### 运行
+ 将编译生成的可执行程序 `luckfox_lvgl_demo` 上传到 Luckfox Pico 上(可使用adb ssh等方式)
+ 板端设置可执行权限后执行
    ```
    chmod a+x luckfox_lvgl_demo
    ./luckfox_lvgl_demo
    ```
## Ubuntu
### 编译
+ 设置环境变量
    ```
    export GLIBC_COMPILER=< glibc 交叉编译工具地址和前缀名 >
    # 例如：export GLIBC_COMPILER=/opt/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-  
    ```
    **注意**：使用绝对地址。    
+ 使用 CMake 编译获取可执行程序 
    ```
    mkdir build
    cd build
    cmake ..
    make -j
    make install
    ```
### 运行
+ 将编译生成的文件夹 `< Project Path >/install/luckfox_lvgl_demo` **整个**上传到 Luckfox Pico 上(可使用adb ssh等方式)
    
    **注意**：由于运行需要依赖动态库，上传文件时可能会丢失软链接，建议压缩上传。
+ 板端设置可执行权限后执行
    ```
    cd luckfox_lvgl_demo
    chmod a+x luckfox_lvgl_demo
    ./luckfox_lvgl_demo
    ```

## 详细
[RGB 屏幕](https://wiki.luckfox.com/zh/Luckfox-Pico/Luckfox-Pico-Ultra-RGB-Screen)


