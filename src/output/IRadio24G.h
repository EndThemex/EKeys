/*
 * IRadio24G.h
 *
 * 2.4G 无线模块抽象（FEATURE_DOC §4，阶段 07 任务 7.1）。
 *
 * 具体射频芯片（nRF24L01+ / 其他）由硬件决定：
 *   - 硬件到位后实现本接口并注入 Wireless24GKeyboardImpl；
 *   - 硬件未到位（BOARD_HAS_24G 未定义）时不实例化，
 *     Wireless24GKeyboardImpl::begin() 返回 false 走 USB 回退。
 */

#ifndef EKEYS_OUTPUT_I_RADIO_24G_H
#define EKEYS_OUTPUT_I_RADIO_24G_H

#include <stdint.h>

namespace ekeys
{

  class IRadio24G
  {
  public:
    virtual ~IRadio24G() = default;

    /* 初始化射频模块（SPI / 配对信道等） */
    virtual bool begin() = 0;

    /* 关闭射频（模式切换时调用） */
    virtual void end() = 0;

    /*
     * 发送 Boot Keyboard 报告：
     *   modifier 为修饰键位（0x01=LCtrl .. 0x80=RWin），
     *   keys[6] 为普通键 keycode（0 表示空位）。
     */
    virtual bool sendKeyboardReport(uint8_t modifier, const uint8_t keys[6]) = 0;

    /* 发送 Consumer 控制报告（多媒体键 usage code） */
    virtual bool sendConsumerReport(uint16_t usage) = 0;

    /* 接收端是否在线（双向链路 established） */
    virtual bool isConnected() const = 0;
  };

} // namespace ekeys

#endif // EKEYS_OUTPUT_I_RADIO_24G_H
