# 编译、烧录与运行

> 适用硬件：ESP32-S3-WROOM-1 (N16R8，16 MB Flash / 8 MB OPI PSRAM)
> 框架：Arduino + PlatformIO 6.x

本文汇总从环境准备到固件上电运行的全部命令。日常开发只需按顺序执行对应章节即可。

---

## 1. 前置准备

| 工具                      | 版本 / 说明                                                                                                                                       |
| ------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| **PlatformIO Core (CLI)** | 6.x，可通过 `pip install platformio` 安装，也可使用 VS Code / Cursor 插件                                                                         |
| **Python**                | 仅 PlatformIO 需要，3.x 即可                                                                                                                      |
| **Git**                   | 拉取 `lib_deps` 里的 GitHub 依赖（BLE-Keyboard / simpleini / ESP32Encoder 等）                                                                    |
| **USB 驱动**              | ESP32-S3 内置 USB CDC，Win10/11 通常免驱；Win7/8.1 需安装 [CP210x / CH343 驱动](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) |
| **数据线**                | USB Type-C，支持数据（非纯充电线）                                                                                                                |

确认 USB 数据线连接后，设备管理器中出现新的 COM 端口（Linux/macOS 为 `/dev/ttyACM*` / `/dev/cu.usbmodem*`）。

---

## 2. 克隆与首次构建

```bash
git clone <repo-url> EKeys
cd EKeys
pio pkg install        # 可选：提前下载 platform / 依赖，避免编译时等待
pio run -e esp32-s3-wroom-1-n16r8
```

首次编译耗时较长（需下载 `espressif32@6.8.0` 平台与若干 `lib_deps`）。后续增量编译通常 < 30s。

> 不主动编译：本项目不主动执行 `pio run`，除非明确要求。

---

## 3. 烧录固件

```bash
# 自动检测串口（推荐）
pio run -e esp32-s3-wroom-1-n16r8 -t upload

# 或显式指定端口
pio run -e esp32-s3-wroom-1-n16r8 -t upload --upload-port COM7        # Windows
pio run -e esp32-s3-wroom-1-n16r8 -t upload --upload-port /dev/ttyACM0 # Linux
```

烧录参数见 [`platformio.ini`](../platformio.ini)：

- `flash_mode=dio`
- `f_flash=80MHz`
- `flash_size=16MB`
- `upload_speed=921600`
- `board_upload.offset_address=0x20000`

> 烧录失败时按 `BOOT` 键重新上电进入下载模式，或按住 `BOOT` 再短按 `RESET`。

---

## 4. 上传 SPIFFS 资源（config / keymap）

```bash
pio run -e esp32-s3-wroom-1-n16r8 -t uploadfs
```

源文件位于 [`data/`](../data/)（`config.ini`、各 `keymapN.ini`、profile 图标、音效等）。

---

## 5. 串口监视器

```bash
pio device monitor -b 115200                          # 默认端口
pio device monitor -b 115200 -p COM7                  # Windows
pio device monitor -b 115200 -p /dev/ttyACM0          # Linux
```

由于启用了 `ARDUINO_USB_CDC_ON_BOOT=1`，日志通过 USB CDC 输出；UART0 仅作烧录。

---

## 6. 擦除 Flash（首次 / 恢复出厂）

```bash
pio run -e esp32-s3-wroom-1-n16r8 -t erase
pio run -e esp32-s3-wroom-1-n16r8 -t uploadfs   # SPIFFS 会被擦除，需重新 uploadfs
pio run -e esp32-s3-wroom-1-n16r8 -t upload
```

---

## 7. 完整首次上电流程

```bash
# 1. 编译并烧录固件
pio run -e esp32-s3-wroom-1-n16r8 -t upload

# 2. 上传 SPIFFS 资源（首次上电 SPIFFS 为空会 LOG_ERROR 后死循环）
pio run -e esp32-s3-wroom-1-n16r8 -t uploadfs

# 3. 打开监视器观察日志
pio device monitor -b 115200
```

> 顺序反了也可以，但 `uploadfs` 必须在 `upload` 之后再次执行以覆盖被 `erase` 清空（仅 `erase` 后）的 SPIFFS。

---

## 8. 常见编译/烧录问题

| 现象                 | 排查                                                                                                                    |
| -------------------- | ----------------------------------------------------------------------------------------------------------------------- |
| `UnicodeDecodeError` | [`partitions-16MB.csv`](../partitions-16MB.csv) 顶部含中文注释，Windows zh-CN 系统默认 GBK 解码。删除注释或改为 ASCII。 |
| 烧录超时 / 失败      | 按住 `BOOT` 短按 `RESET` 进入下载模式；降低 `upload_speed` 至 115200                                                    |
| 找不到串口           | 确认数据线支持数据；Win7 安装 CP210x 驱动；确认 `ARDUINO_USB_CDC_ON_BOOT=1`                                             |
| 首次上电黑屏         | 检查 LCD 排线、SPI 接线；查看串口日志是否 `NV3007 init failed`                                                          |
| 首次上电死循环       | 日志出现 `SPIFFS mount failed` 之类 → 先执行 `pio run -t uploadfs`                                                      |
