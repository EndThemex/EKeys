/*
 * SerialProtocol.h
 *
 * 私有协议传输层（FEATURE_DOC §5.1；阶段 04 任务 4.3）。
 *
 * - 本阶段仅 USB CDC Serial（TCP 于阶段 06 接入，届时复用同一解析分发）
 * - JSON 行协议：请求 {"cmd":0x07,"seq":1,"data":{...}}，响应 cmd|0x80
 * - 心跳 0x0a 自处理：直接回复 0x8a
 * - 日志与协议共用 USB CDC（LogManager），桌面 App 需跳过非 '{' 开头的行
 */

#ifndef EKEYS_PROTOCOL_SERIAL_PROTOCOL_H
#define EKEYS_PROTOCOL_SERIAL_PROTOCOL_H

#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>

namespace ekeys {

/* 命令清单（FEATURE_DOC §5.3） */
enum CommandType : uint8_t {
    CMD_CONF_VERSION_GET = 0x01,
    CMD_CONF_VERSION_SET = 0x02,
    CMD_DEVICE_INFO_GET  = 0x03,
    CMD_DEVICE_INFO_SET  = 0x04,
    CMD_KEYMAP_GET       = 0x05,
    CMD_KEYMAP_SET       = 0x06,
    CMD_CONFIG_GET       = 0x07,
    CMD_CONFIG_SET       = 0x08,
    CMD_KEY_EVENT        = 0x09,
    CMD_HEARTBEAT        = 0x0a,
    CMD_FIRMWARE_INFO    = 0x0b,
    CMD_VOICE_TEXT       = 0x0c,
    CMD_PC_STATUS        = 0x0d,
    CMD_MUSIC_STATUS     = 0x0e,
    CMD_MUSIC_CONTROL    = 0x0f,
    CMD_PROFILE_STATE    = 0x10,
    CMD_PROFILE_ICON_SET = 0x11,
    CMD_HA_STATUS        = 0x12,
};

class SerialProtocol {
public:
    static SerialProtocol &instance();

    SerialProtocol(const SerialProtocol &) = delete;
    SerialProtocol &operator=(const SerialProtocol &) = delete;

    /* 幂等；Serial 由 main.cpp 提前 Serial.begin(115200) */
    void begin();

    /* MainTask::loop() 周期调用：流式收字节，拼成行后解析分发 */
    void poll();

    /* 把构建好的响应文档序列化为 JSON 行发出 */
    void sendDocument(JsonDocument &doc);

    /* 通用成功响应：{"cmd":cmd|0x80,"seq":N,"status":0,"data":{...}} */
    void sendSuccessResponse(int original_cmd, int seq, JsonObject data);

    /* 错误响应：{"cmd":cmd|0x80,"seq":N,"status":1,"error":"..."} */
    void sendErrorResponse(int original_cmd, int seq, const char *error);

    static bool isResponseCommand(int cmd) { return (cmd & 0x80) != 0; }

private:
    SerialProtocol() = default;

    void handleLine(char *line);
    void sendHeartbeatResponse(int seq);

    static constexpr size_t kLineBufferSize = 2048;

    char   line_[kLineBufferSize];
    size_t line_len_ = 0;
    bool   overflow_ = false;  // 超长行丢弃直到换行
    bool   begun_ = false;
};

}  // namespace ekeys

#endif  // EKEYS_PROTOCOL_SERIAL_PROTOCOL_H
