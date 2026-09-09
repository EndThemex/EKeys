/*
 * usbmic_proto.cpp
 *
 * M0 原型验证：USB 麦克风（2026-09-09，方案见对话记录 / 拟 docs/09）。
 *
 *   目的：不动主固件，用独立 env（usbmic-proto，pioarduino + arduino 3.3.11）验证：
 *     1. arduino-esp32 3.3.11 / IDF 5.5.5 工具链下 USBAudioCard 可编译链接
 *     2. UAC1 仅麦克风设备（UAC_SPK_NONE）在 Windows 上能枚举并真正录音
 *        （3.3.8 有 dwc2 ISO IN bug：能枚举、录音不启动，tinyusb #3640 已修）
 *     3. ICS43434（I2S0：SCK=13 / WS=12 / SD=14）在 3.x 新 I2S API（ESP_I2S.h）
 *        下 48kHz/16bit/mono 采集正常（旧代码用的 driver/i2s.h 在 IDF5 已删除）
 *
 *   观察：USB 接 PC → 设备管理器出现麦克风 → Audacity/ffmpeg 选本设备录音；
 *         日志走 UART0（TXD0/RXD0 测试点，USB-UART 适配器，115200）看
 *         接口 enable 事件与 5s 统计（CDC 已关：UAC+CDC 复合设备在
 *         Windows 上 usbaudio.sys 启动失败，见 env 注释）。
 *
 *   隔离手段：如需区分「I2S 采集问题」与「USB 传输问题」，编译时加
 *         -DEKEYS_PROTO_TONE_TEST=1 改发 1kHz 正弦信号。
 *
 *   依据：arduino-esp32 官方例程 libraries/USB/examples/AudioCard（3.3.11）。
 *   启动顺序：先配 I2S → 注册回调 + 启动音频设备 → 最后 USB.begin()。
 */

#include <Arduino.h>
#include <math.h>

#include "ESP_I2S.h"
#include "USB.h"
#include "USBAudioCard.h"

namespace
{
  /* 与 src/hardware/PinMap.h 的 I2S 麦克风引脚一致（不 include PinMap.h，
   * 避免 LDF 把 GFX 库拉进原型构建） */
  constexpr int kMicBclk = 13; // ICS43434 SCK（kPinI2sMicSck）
  constexpr int kMicWs = 12;   // ICS43434 WS（kPinI2sMicWs）
  constexpr int kMicDin = 14;  // ICS43434 SD → 主控输入（kPinI2sMicSd）

  constexpr uint32_t kSampleRate = 48000;

#ifndef EKEYS_PROTO_TONE_TEST
#define EKEYS_PROTO_TONE_TEST 0
#endif
} // namespace

/* 仅麦克风：无扬声器（UAC_SPK_NONE）、单声道 */
USBAudioCard uac(kSampleRate, UAC_BPS_16, UAC_SPK_NONE, UAC_MIC_MONO);
I2SClass i2s;

namespace
{
  uint8_t s_buf[1024];
  volatile bool s_mic_enabled = false;
  uint32_t s_write_bytes = 0;
  uint32_t s_read_empty = 0;
  uint32_t s_last_stat_ms = 0;
#if EKEYS_PROTO_TONE_TEST
  uint32_t s_phase_q16 = 0;
#endif

  /* USB 链路 + UAC 控制（音量/静音/采样率/接口开关）统一回调 */
  void usbEventCb(void *, esp_event_base_t base, int32_t id, void *arg)
  {
    if (base == ARDUINO_USB_EVENTS)
    {
      arduino_usb_event_data_t *data = (arduino_usb_event_data_t *)arg;
      switch (id)
      {
      case ARDUINO_USB_STARTED_EVENT:
        Serial.println("USB PLUGGED");
        break;
      case ARDUINO_USB_STOPPED_EVENT:
        Serial.println("USB UNPLUGGED");
        break;
      case ARDUINO_USB_SUSPEND_EVENT:
        Serial.println("USB SUSPENDED");
        break;
      case ARDUINO_USB_RESUME_EVENT:
        Serial.println("USB RESUMED");
        break;
      default:
        break;
      }
    }
    else if (base == ARDUINO_USB_AUDIO_CARD_EVENTS)
    {
      arduino_usb_audio_card_event_data_t *data =
          (arduino_usb_audio_card_event_data_t *)arg;
      switch (id)
      {
      case ARDUINO_USB_AUDIO_CARD_MUTE_EVENT:
        Serial.printf("AUDIO MUTE CH:%d MUTED:%d\r\n",
                      data->mute.channel, data->mute.muted);
        break;
      case ARDUINO_USB_AUDIO_CARD_SAMPLE_RATE_EVENT:
        Serial.printf("AUDIO SAMPLE RATE: %lu\r\n",
                      (unsigned long)data->sample_rate.rate);
        break;
      case ARDUINO_USB_AUDIO_CARD_INTERFACE_ENABLE_EVENT:
        /* 例程约定：interface 非 0 = MIC */
        s_mic_enabled = data->interface_enable.interface &&
                        data->interface_enable.enable;
        Serial.printf("AUDIO INTERFACE: %s ENABLED: %d\r\n",
                      data->interface_enable.interface ? "MIC" : "SPK",
                      data->interface_enable.enable);
        break;
      default:
        break;
      }
    }
  }

#if EKEYS_PROTO_TONE_TEST
  /* 1kHz 正弦，16bit mono，96 字节/毫秒 */
  size_t fillTone()
  {
    const size_t frames = kSampleRate / 1000; // 48
    int16_t *p = (int16_t *)s_buf;
    for (size_t i = 0; i < frames; ++i)
    {
      /* 相位累加：freq * 2^16 / sample_rate */
      s_phase_q16 += (1000u << 16) / kSampleRate;
      p[i] = (int16_t)(sinf(s_phase_q16 * 6.2831853f / 65536.0f) * 8000.0f);
    }
    return frames * sizeof(int16_t);
  }
#endif
} // namespace

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("\r\n===== USB mic prototype (M0) =====");

  i2s.setPins(kMicBclk, kMicWs, -1 /*无 dout*/, kMicDin);
  /* 麦克风 L/R 接 WS，左右时隙同数据；与旧固件 ONLY_LEFT 语义一致取左时隙 */
  if (!i2s.begin(I2S_MODE_STD, kSampleRate, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT))
  {
    Serial.println("I2S begin FAILED");
  }
  else
  {
    Serial.println("I2S 48k/16bit/mono ready");
  }

  uac.onEvent(usbEventCb);
  uac.begin();
  USB.onEvent(usbEventCb);
  /* 默认 PID 0x0002 与主固件(CDC+HID)相同，Windows 会复用旧设备实例的驱动
   * 绑定；换 PID 强制按新设备重装驱动（排除代码 10 的缓存因素） */
  USB.PID(0x4034);
  USB.productName("EKeys USB Mic Proto");
  USB.begin();
  s_last_stat_ms = millis();
}

void loop()
{
  /* 每轮读 ~1ms（96 字节）麦克风 PCM 送 USB IN 端点 */
  size_t to_read =
      (uac.sampleRate() * uac.micChannels() * uac.bytesPerSample()) / 1000;
  if (to_read > sizeof(s_buf))
  {
    to_read = sizeof(s_buf);
  }

  size_t got = 0;
#if EKEYS_PROTO_TONE_TEST
  got = fillTone();
#else
  got = i2s.readBytes((char *)s_buf, to_read);
  if (got == 0)
  {
    s_read_empty++;
  }
#endif
  if (got > 0)
  {
    uac.write(s_buf, (uint16_t)got);
    s_write_bytes += got;
  }

  /* 5s 统计：48k*2B 期望 ~96KB/s，明显偏低说明 I2S 断流 */
  uint32_t now = millis();
  if (now - s_last_stat_ms >= 5000)
  {
    /* s_write_bytes*1000 会溢出 uint32（>4.3MB 累计即错），用 uint64 运算 */
    Serial.printf("STAT mic_en=%d write=%luKB (%lu B/s) i2s_empty=%lu\r\n",
                  s_mic_enabled ? 1 : 0,
                  (unsigned long)(s_write_bytes / 1024),
                  (unsigned long)((uint64_t)s_write_bytes * 1000UL / (now - s_last_stat_ms)),
                  (unsigned long)s_read_empty);
    s_read_empty = 0;
    s_last_stat_ms = now;
  }
}
