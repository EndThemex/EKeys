# EKeys 硬件测试程序 — 任务规划

> 目标：一个独立于主固件的裸机测试程序，覆盖 **屏幕刷新 / 矩阵按键 / EC11 旋钮 / 麦克风 / 喇叭** 五项硬件功能（2026-09-06 按需求移除屏幕显示与 RGB LED 测试项）。
> 复用生产驱动（`DisplayDriver / MatrixScanner / RGBDriver / RotaryEncoder / Mic / Backlight`），测的就是主固件实际使用的代码路径。
> 引脚以 `PINOUT.md` 与 [src/hardware/PinMap.h](../src/hardware/PinMap.h) 为准，本程序不改动任何生产代码。

***

## 1. 总体方案

### 1.1 独立 PlatformIO 环境（不动主固件）

新增 `[env:hwtest]`，用 **白名单 `build_src_filter`** 只编译 `src/hwtest/` 测试代码 + 复用的驱动文件；`src/main.cpp` 与 app/config/network/protocol 等全部不参与编译。

```
烧录测试程序：  pio run -e hwtest -t upload
烧回主固件：    pio run -e esp32-s3-wroom-1-n16r8 -t upload
```

为避免 `pio run` 不带 `-e` 时把两个环境都构建，补一个 `[platformio] default_envs = esp32-s3-wroom-1-n16r8`（主固件仍是默认构建目标）。

### 1.2 platformio.ini 配置组织（公共段重构）

当前所有配置写在一个 env 段里。为让 hwtest 继承同一套 flash/PSRAM/分区/构建参数（避免两处维护、复现 QIO/DIO 之类的踩坑），重构成标准继承结构：

```ini
[platformio]
default_envs = esp32-s3-wroom-1-n16r8

[env]                       ; 公共配置：board / flash(DIO!) / PSRAM(dio_opi) /
                            ; partitions / offset_address / build_flags / lib_deps / monitor
[env:esp32-s3-wroom-1-n16r8] ; 仅 build_src_filter（主固件 +<*> 与字体排除）
[env:hwtest]                 ; 仅 build_src_filter（测试白名单）
```

**验证要求**：重构后用 `pio project config -e esp32-s3-wroom-1-n16r8` 核对，主环境最终生效值必须与重构前逐项一致（尤其 `board_build.flash_mode = dio`、`board_upload.offset_address = 0x20000`、`memory_type = dio_opi`）。

### 1.3 hwtest 编译白名单

```ini
build_src_filter =
    +<hwtest/*>
    +<display/DisplayDriver.cpp>
    +<display/Backlight.cpp>
    +<input/MatrixScanner.cpp>
    +<input/RotaryEncoder.cpp>
    +<rgb/RGBDriver.cpp>
    +<audio/Mic.cpp>
    +<logging/LogManager.cpp>
```

说明：
- `RotaryEncoder` 头文件带 `lvgl.h`（仅用 `LV_KEY_*` 常量），`include/lv_conf.h` + `-DLV_CONF_INCLUDE_SIMPLE` 已在公共 build_flags 中，可正常编译；测试环境不初始化 LVGL。
- **不**复用 `Speaker.cpp`（它依赖 `VoiceRecognizer`，会拖进一大串主固件依赖）。喇叭测试直接用 `driver/i2s.h` 在 **I2S_NUM_1** 上按同一组引脚（BCLK=IO10 / LRCLK=IO9 / DIN=IO11）输出音调，等价验证 MAX98357 硬件链路。

### 1.4 新增文件（全部在 `src/hwtest/`，单个文件保持小而专一）

| 文件 | 职责 |
| --- | --- |
| `main_hwtest.cpp` | setup/loop：5V 使能 → 背光/屏幕 → 各驱动 begin → 菜单状态机入口 |
| `HwTestMenu.h/.cpp` | 测试项菜单（旋钮选择 / SW 进入退出）与状态机框架 |
| `HwTestUI.h/.cpp` | 公共绘制助手：标题栏、清屏、文本行、级别条（直接用 Arduino_GFX API） |
| `TestPerf.h/.cpp` | T1 屏幕刷新（fillScreen FPS、区域刷新 FPS） |
| `TestKeys.h/.cpp` | T2 矩阵按键（11 键网格高亮 + 事件计数 + 联动 RGB） |
| `TestRotary.h/.cpp` | T3 旋钮（计数/方向/单击/双击） |
| `TestMic.h/.cpp` | T4 麦克风（实时电平条 + 自动录音回放循环） |
| `TestSpeaker.h/.cpp` | T5 喇叭（I2S1 正弦扫频 + 音量阶梯 + 序列指示条） |

***

## 2. 交互设计

- **开机**：自检信息屏（flash/PSRAM 大小、复用驱动逐一 begin 的结果），1.5s 后进菜单。
- **菜单**：5 个测试项列表，**旋钮旋转** = 移动高亮，**SW 单击** = 进入当前项，测试项内 **SW 双击** = 返回菜单。
- **串口**：COM11（USB CDC）115200，与屏幕输出同步打印日志，方便记录测量数据。
- 每个测试项自带循环刷新，`MatrixScanner::scan()` 与 `RotaryEncoder::loop()` 在主循环里持续喂（5ms 节流），保证测试项内按键/旋钮始终可用（如 T1 刷新测试用 SW 启动各步骤）。

## 3. 测试项设计

### T1 屏幕刷新 `TestPerf`

| 步骤 | 内容 | 输出 |
| --- | --- | --- |
| 1 | `fillScreen` 连续 100 帧（红蓝交替）计时 | FPS（屏幕 + 串口） |
| 2 | 全屏 1/8 区域矩形填充 300 帧 | 区域刷新 FPS |
| 3 | `draw16bitRGBBitmap` 整屏位图搬运 100 帧 | 位图 FPS（逼近 SPI@40MHz 上限 ≈ 40 FPS） |

理论参考：142×428×16bit ≈ 97KB/帧，40MHz SPI ≈ 24ms/帧 → 全屏填充上限 ~40 FPS。实测值显著偏低（< 30 FPS）需排查。

### T2 矩阵按键 `TestKeys`

- 屏幕画 3×4 键位网格（ROW0-COL3 空位画"×"），按下高亮 + 显示计数；松开恢复。
- 每次 press/release 事件串口打印：`keyId / row,col / millis / phase`。
- 按下同时点亮对应 WS2812（下标 = keyId-1），实现按键↔RGB 联动互验。
- 通过标准：11 键逐一测试无漏检、无粘滞；快速连按无误触（10ms 消抖生效）。

### T3 旋钮 `TestRotary`

- 屏幕中央大号显示累计计数值，左转 -1 / 右转 +1（用生产 `RotaryEncoder` 回调 `LV_KEY_LEFT/RIGHT`），方向箭头指示。
- SW **单击**计数、**双击**计数分行显示（`LV_KEY_ENTER / LV_KEY_ESC`）。
- 通过标准：不丢步、方向不反（`RotaryEncoder` 内部阈值=2 半齿）、无抖动连发。

### T4 麦克风 `TestMic`

自动循环流程（尽量少按键，便于反复验证）：

| 步骤 | 内容 |
| --- | --- |
| 1 | 进入即 `Mic::begin()`（I2S0 RX，16kHz/16bit/mono，SCK=IO13 专用 / WS=IO12 / SD=IO14）→ 实时 VU |
| 2 | 实时 VU：每帧 `Read` 512 样本 → RMS + 峰值 → 大电平条 + dBFS 数字 + 峰值保持线 |
| 3 | SW 单击录音：录 3s 到 PSRAM（96KB），大倒计时数字 + 进度条 |
| 4 | 录完自动回放（`Mic::end()` 后 I2S1 播放），进度条显示；SW 单击可跳过 |
| 5 | 播完自动回到实时 VU，无需手动重启 |
| 6 | SW 双击退出，`Mic::end()` 兜底 |

通过标准：说话时电平条明显起伏、静音时底噪低；回放清晰、音量足够、无明显失真。

### T5 喇叭 `TestSpeaker`

- `driver/i2s.h` 直接配 I2S_NUM_1 TX：BCLK=IO10 / LRCLK=IO9 / DIN=IO11（主控侧数据输出），16bit 立体声（双声道同数据），生成正弦波缓冲循环 `i2s_write`。
- 音调序列：440Hz → 1kHz → 2kHz → 4kHz（各 1s，大字显示当前频率）。
- 音量阶梯：1kHz 下幅度 25% → 50% → 75% → 100%。
- 屏幕底部 8 步序列指示条（当前步高亮）+ 实时进度条。
- SW 单击跳过当前步，SW 双击提前结束，`i2s_driver_uninstall` 兜底。
- 通过标准：各频段出声均匀无破音，幅度阶梯可闻线性变化。

## 4. 实施顺序（任务分解）

| # | 任务 | 产出 |
| --- | --- | --- |
| 1 | platformio.ini 公共段重构 + `[platformio] default_envs` + 核对主环境配置不变 | 可双环境构建 |
| 2 | `src/hwtest/` 骨架：main_hwtest.cpp + HwTestMenu + HwTestUI（开机自检屏 + 菜单可转可点） | 能编译烧录、屏幕出菜单 |
| 5 | T4 麦克风 + T5 喇叭 | 全部 5 项可用 |
| 6 | 全流程自测清单执行（见 §5），修问题 | 通过验收 |

> 按项目规则**不主动编译**；每个阶段完成后由用户执行 `pio run -e hwtest` 确认。

## 5. 验收清单

- [ ] `pio run -e hwtest` 编译零错误；主环境 `pio run -e esp32-s3-wroom-1-n16r8` 编译不受影响
- [ ] 主环境 `pio project config` 输出与重构前一致（flash_mode=dio、offset=0x20000、dio_opi）
- [ ] T1~T5 全部按 §3 标准执行并通过
- [ ] 测试程序烧录后可正常烧回主固件（`pio run -e esp32-s3-wroom-1-n16r8 -t upload`）

## 6. 风险与注意事项

1. **I2S 引脚全部独立（2026-09-06 原理图确认）**：功放 BCLK=IO10 / LRCLK=IO9 / DIN=IO11，麦克风 SCK=IO13 / WS=IO12 / SD=IO14，无共用线。曾有两处误配：时钟误用 IO10 共用线（录音全零）、SD/DIN 数据线对调（录音与播放双向无声），2026-09-06 均已按原理图更正。测试程序仍按测试项串行执行 + 显式 `i2s_driver_install/uninstall`，防残留驱动。
2. **IO3 = 5V 升压使能**：主固件未见显式驱动该引脚（仅 PinMap 定义）。测试程序 setup() 直接拉高，确保 RGB / 功放等 5V 负载供电稳定（与主固件行为无冲突）。
3. **GPIO46（ROW0）为 strapping 引脚**：沿用主固件矩阵扫描配置（输入上拉），启动时序与主固件相同，无额外风险。
4. **烧录环境与偏移**：两个 env 共用同一分区表与 `offset_address=0x20000`，上传路径与主固件一致，无新偏移风险。
5. **测试程序不自动恢复主固件**：测完需重新烧录主固件，文档与屏幕退出菜单处均有提示。
6. **SPI 频率**：沿用 40MHz（`DisplayDriver::begin(40000000)` 默认值），不测试更高频率，避免引入本板闪存/走线相关的时序变量。
