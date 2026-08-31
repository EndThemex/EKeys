/*
 * SerialProtocol.cpp
 *
 * 见 SerialProtocol.h。
 */

#include "SerialProtocol.h"

#include <Arduino.h>
#include <string.h>

#include "CommandRegistry.h"
#include "logging/LogManager.h"

namespace ekeys {

SerialProtocol &SerialProtocol::instance()
{
    static SerialProtocol inst;
    return inst;
}

void SerialProtocol::begin()
{
    if (begun_) {
        return;
    }
    line_len_ = 0;
    overflow_ = false;
    begun_ = true;
    LOG_INFO("PROTO", "SerialProtocol ready (USB CDC, 115200, JSON lines)");
}

void SerialProtocol::poll()
{
    if (!begun_) {
        return;
    }

    while (Serial.available() > 0) {
        char c = static_cast<char>(Serial.read());

        if (c == '\n') {
            if (overflow_) {
                overflow_ = false;
                line_len_ = 0;
                continue;
            }
            line_[line_len_] = '\0';
            handleLine(line_);
            line_len_ = 0;
        } else if (c == '\r') {
            continue;
        } else if (line_len_ + 1 >= kLineBufferSize) {
            overflow_ = true;
            LOG_WARNING("PROTO", "line overflow, discarded");
        } else {
            line_[line_len_++] = c;
        }
    }
}

void SerialProtocol::handleLine(char *line)
{
    /* 跳过行首空白；空行 / 日志回显等非 JSON 内容直接忽略 */
    while (*line == ' ' || *line == '\t') {
        ++line;
    }
    if (*line == '\0' || *line != '{') {
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, line);
    if (err) {
        LOG_WARNING("PROTO", "json parse error: %s", err.c_str());
        sendErrorResponse(0, 0, "json parse error");
        return;
    }

    if (doc["cmd"].isNull()) {
        sendErrorResponse(0, 0, "missing 'cmd' field");
        return;
    }

    const int cmd = doc["cmd"].as<int>();
    const int seq = doc["seq"] | 0;

    /* 响应包（cmd|0x80）本阶段不处理 */
    if (isResponseCommand(cmd)) {
        return;
    }

    /* 心跳自处理（FEATURE_DOC §5.3：0x0a 由 SerialProtocol 处理） */
    if (cmd == CMD_HEARTBEAT) {
        sendHeartbeatResponse(seq);
        return;
    }

    JsonObject data = doc["data"].as<JsonObject>();
    const int rc = CommandRegistry::instance().dispatch(cmd, seq, data);
    if (rc == -1) {
        sendErrorResponse(cmd, seq, "unknown command");
    } else if (rc != 0) {
        sendErrorResponse(cmd, seq, "command failed");
    }
}

void SerialProtocol::sendDocument(JsonDocument &doc)
{
    serializeJson(doc, Serial);
    Serial.print('\n');
}

void SerialProtocol::sendSuccessResponse(int original_cmd, int seq,
                                         JsonObject data)
{
    JsonDocument doc;
    doc["cmd"] = original_cmd | 0x80;
    doc["seq"] = seq;
    doc["status"] = 0;
    JsonObject out = doc["data"].to<JsonObject>();
    for (JsonPair kv : data) {
        out[kv.key()] = kv.value();
    }
    sendDocument(doc);
}

void SerialProtocol::sendErrorResponse(int original_cmd, int seq,
                                       const char *error)
{
    JsonDocument doc;
    doc["cmd"] = original_cmd | 0x80;
    doc["seq"] = seq;
    doc["status"] = 1;
    doc["error"] = error ? error : "unknown error";
    sendDocument(doc);
}

void SerialProtocol::sendHeartbeatResponse(int seq)
{
    JsonDocument doc;
    doc["cmd"] = CMD_HEARTBEAT | 0x80;
    doc["seq"] = seq;
    doc["status"] = 0;
    JsonObject data = doc["data"].to<JsonObject>();
    data["timestamp"] = millis();
    data["device"] = "EKeys";
    sendDocument(doc);
}

}  // namespace ekeys
