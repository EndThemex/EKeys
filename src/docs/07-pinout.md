# EKeys 引脚总览

> 参考项目：[FunModularKeyboard](../docs/)（同平台、同芯片）。
> 全部引脚按"参考 → 本项目使用"两列对照，便于核对。

## 1. 芯片与开发板

| 项目 | 值 |
|---|---|
| 板子 | ESP32-S3-DevKitC-1 |
| 平台 | espressif32@6.4.0 |
| 框架 | arduino |
| Flash | 8 MB（QD，无 PSRAM） |

## 2. 显示屏 NV3007（SPI）

| 信号 | FunModularKeyboard | EKeys (本项目) | 备注 |
|---|---|---|---|
| MOSI | 7 | **42** | 主 SPI |
| SCLK | 6 | **41** | 主 SPI |
| CS   | 5 | **40** | 片选 |
| DC   | 4 | **45** | 数据/命令 |
| RST  | 8 | **46** | 复位 |
| BL   | 37 | **37** | 背光（active-LOW） |
| SPI 频率 | 5 MHz | **10 MHz** | EKeys 用更高 SPI 频率 |

> 说明：FunModularKeyboard 用 `TFT_BL=37` 与本项目一致；其他 SPI 引脚在本项目里被腾出给矩阵 / 编码器 / RGB 灯，所以重映射到 40/41/42/45/46。
> 详细屏幕参数（旋转、offset 等）见 `01-keyscan-overview.md` 与 `main.cpp` 中 `Arduino_NV3007` 构造。

## 3. 矩阵键盘（3×4 = 9 键）

参考 FunModularKeyboard 的 `MatrixScanner.h`：

| | FunModularKeyboard (5×4) | EKeys (3×4) |
|---|---|---|
| 行 | `{48, 10, 47, 33, 14}` | **`{48, 10, 47}`**（取前 3 行） |
| 列 | `{35, 34, 7, 13}` | **`{35, 34, 7, 13}`**（完全沿用） |

### 键位 ↔ keyId 映射（梯形布局：2/3/4）

```
        COL0  COL1  COL2  COL3
ROW0:    1     2     -     -     ← 2 键
ROW1:    -     3     4     5     ← 3 键
ROW2:    6     7     8     9     ← 4 键
```

`-` 表示该位置无按键（keyMap 填 0 跳过扫描）。

## 4. EC11 旋转编码器

| 信号 | FunModularKeyboard | EKeys |
|---|---|---|
| CLK (A 相) | `ENCODER_CLK  = 5`  | **5** |
| DT  (B 相) | `ENCODER_DT   = 21` | **21** |
| SW  (按键) | `ENCODER_SW   = GPIO_NUM_9` | **9** |

> 库：`madhephaestus/ESP32Encoder@^0.11.7`（与 FunModularKeyboard 同源，新版）。

## 5. RGB 灯（WS2812B，单线 DIN）

| 信号 | FunModularKeyboard | EKeys |
|---|---|---|
| 数据 DIN | `LED_PIN = 6` | **6** |
| VCC 控（MOSFET）| `LED_VCC_CTRL = 36` | **36** |
| 灯珠数 | `NUM_LEDS = 16` | **`NUM_LEDS = 9`**（与按键 1:1） |
| 灯型 | `WS2812B` | `WS2812B` |
| 色彩序 | `GRB` | `GRB` |

> 库：`fastled/FastLED@^3.10.1`。
> `LED_VCC_CTRL = 36` 为高电平开 VCC；本项目直接常开（参考 FunModularKeyboard 也是 `digitalWrite(LOW)` —— 注意：他们写的是 LOW，但用 pin36 控 MOSFET 时需查电路是 active-H 还是 active-L。本项目沿用同样设置；若实际不亮，需对调）。

### keyId ↔ LED 索引映射

```
keyId 1 2 3 4 5 6 7 8 9
LED    0 1 2 3 4 5 6 7 8
```

`RGBLightControl` 内部 LED 索引 0..8，与 keyId 差 1：`ledIndex = keyId - 1`。

## 6. 引脚冲突检查

| GPIO | 占用 | 与其他模块冲突？ |
|---|---|---|
| 5  | EC11 CLK | 无 |
| 6  | RGB DIN | 无（FunModularKeyboard 原 6=SCLK 现空出）|
| 7  | 矩阵 COL2 | 无 |
| 9  | EC11 SW | 无 |
| 10 | 矩阵 ROW1 | 无 |
| 13 | 矩阵 COL3 | 无 |
| 21 | EC11 DT | 无 |
| 34 | 矩阵 COL1 | 无 |
| 35 | 矩阵 COL0 | 无 |
| 36 | RGB VCC 控 | 无 |
| 37 | TFT BL | 无 |
| 40 | TFT CS | 无 |
| 41 | TFT SCLK | 无 |
| 42 | TFT MOSI | 无 |
| 45 | TFT DC | 无 |
| 46 | TFT RST | 无 |
| 47 | 矩阵 ROW2 | ⚠ ESP32-S3-N8 上可能被 flash/PSRAM 占用，待硬件核实 |
| 48 | 矩阵 ROW0 | 无 |

## 7. 关键配置宏（platformio.ini）

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32@6.4.0
board    = esp32-s3-devkitc-1
framework= arduino
lib_deps =
    lvgl/lvgl@8.3.11
    GFX Library for Arduino@1.6.0
    madhephaestus/ESP32Encoder@^0.11.7
    mathertel/OneButton@^2.5.0
    fastled/FastLED@^3.10.1     # 新增：RGB 灯
monitor_speed = 115200
```
