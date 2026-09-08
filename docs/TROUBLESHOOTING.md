# 注意事项与常见问题

本文汇总开发 / 调试过程中需要长期遵守的约束，以及按现象归类的故障排查清单。

> 编译烧录命令与流程见 [`COMPILING.md`](./COMPILING.md)；项目结构见 [`PROJECT_LAYOUT.md`](./PROJECT_LAYOUT.md)。

---

## 1. 硬件相关

### 1.1 Flash 必须使用 DIO

本板厂商 ID 0x46 的闪存在 QIO@80MHz 下 ROM 加载失败（`ets_loader.c:78`），[`platformio.ini`](../platformio.ini) 已显式 `board_build.flash_mode = dio`，**不要改回 QIO**。

### 1.2 `board_upload.offset_address = 0x20000` 必须保留

espressif32 ≥ 6.5 不再解析自定义分区表计算 app 偏移，不显式指定会把固件写到 0x10000 破坏 `phy_init` 分区。

### 1.3 PSRAM 配置

`memory_type=dio_opi` + `psram_type=opi`，匹配 N16R8（Octal PSRAM）。LVGL 帧缓冲分配在 PSRAM 中。

### 1.4 LCD_RST 未分配引脚

阶段 01 末 `kPinLcdRst = GFX_NOT_DEFINED`，`Arduino_GFX::begin()` 跳过软件复位。若硬件改造需软件复位，在 [`src/hardware/PinMap.h`](../src/hardware/PinMap.h) 重指派并同步 [`PINOUT.md`](../PINOUT.md)。

### 1.5 按键矩阵二极管方向

每键串联 1N4148，**正极在列侧**，电流方向 `列→行`。行脚拉低、读列脚上拉电平。方向配反将导致所有按键无响应。

### 1.6 WS2812B 电源极性

`LED_PWR_CTRL` (IO21) 为 P-MOS 高边开关，**拉低导通**（VGS=-3.3V），拉高截止。使能灯条应先拉低 `LED_PWR_CTRL` 再驱动 DIN。

### 1.7 Strapping 引脚

`GPIO3`（5V 使能）、`GPIO45`（INT）、`GPIO46`（ROW0，默认 Boot 模式 strapping）上电时需保持手册规定的电平，避免启动异常。

### 1.8 无触摸屏

硬件无 TP_* 引脚，[`LvglPort.cpp`](../src/display/LvglPort.cpp) 未注册 `lv_indev_*`。UI 操作仅依赖 EC11 旋钮 + 11 键矩阵，SquareLine 生成的 `ButtonLeft/Right/Enter/Exit` 触屏兜底按钮**永远不会被触发**（保留以便未来扩展）。

---

## 2. 软件 / 构建相关

### 2.1 依赖版本固定

[`platformio.ini`](../platformio.ini) 中 `espressif32@6.8.0`、LVGL 8.3.11、GFX Library for Arduino 1.6.0、BLE-Keyboard 指定 commit hash。升级任一版本都可能引入头文件变更。

### 2.2 字体白名单

`build_src_filter` 已排除 `BebasNeueFont32/64/80` 与 `FontCKJGT32/40/48/64/80`，主固件不参与编译。如需新增超大字体需同步检查 PSRAM 占用。

### 2.3 分区表仅 ASCII

[`partitions-16MB.csv`](../partitions-16MB.csv) 顶部注释明确，Windows zh-CN 系统默认 GBK 解码，非 ASCII 注释会在 `checkprogsize` 阶段报 `UnicodeDecodeError`。

### 2.4 首次上电若 SPIFFS 为空

[`src/main.cpp`](../src/main.cpp) 在挂载失败时 LOG_ERROR 后死循环。请先执行 `pio run -t uploadfs` 写入 `data/` 内容。

### 2.5 不主动编译

按项目规则，本项目不主动执行 `pio run`，除非明确要求。

---

## 3. 桌面 App / 协议

- 设备作为 **TCP Server**（默认端口见 [`data/config.ini`](../data/config.ini)），通过局域网发现协议广播；详见 [`docs/desktop-app-protocol.md`](./desktop-app-protocol.md)。
- USB CDC 与 TCP 同时可用，互不冲突。串口监视器 115200 bps。

---

## 4. 开发与维护

- 新增硬件功能：在 [`src/hardware/PinMap.h`](../src/hardware/PinMap.h) 集中定义引脚，并在 [`PINOUT.md`](../PINOUT.md) 同步记录。
- 修改配置项默认值：编辑 [`src/config/DeviceSettings.h`](../src/config/DeviceSettings.h)，**不要** 直接写死在调用处。
- UI 改动：使用 SquareLine Studio 打开工程，生成产物覆盖 [`src/ui/`](../src/ui/)，避免手工改动被生成器覆盖。
- 阶段性计划：在 [`docs/`](../docs/) 对应阶段文档的 `变更记录` 追加条目，按 `NN-阶段名.md` 命名。

---

## 5. 常见问题速查

| 现象 | 排查 |
| --- | --- |
| 上电黑屏 | 检查 LCD 排线、SPI 接线；查看串口日志是否 `NV3007 init failed` |
| USB 不识别 | 确认数据线支持数据；Win7 安装 CP210x 驱动；确认 `ARDUINO_USB_CDC_ON_BOOT=1` |
| 烧录超时 | 按住 `BOOT` 短按 `RESET` 进入下载模式；降低 `upload_speed` 至 115200 |
| 按键全部无响应 | 检查矩阵二极管方向（列→行）；确认 `DEBOUNCE_TIME_MS=10` |
| Wi-Fi 配网失败 | [`data/config.ini`](../data/config.ini) 中 `wifi_switch=1` 且填写正确 SSID / 密码 |
| SPIFFS 相关错误 | 执行 `pio run -t uploadfs`；或在首次烧录后等待 SPIFFS 格式化完成 |
| 编译报 `UnicodeDecodeError` | 删除 [`partitions-16MB.csv`](../partitions-16MB.csv) 顶部非 ASCII 注释 |
| 分区破坏 / 启动异常 | 重新 `pio run -t erase && -t uploadfs && -t upload` |

更多历史问题已沉淀在 [`.trae/rules/rules.md`](../.trae/rules/rules.md)。
