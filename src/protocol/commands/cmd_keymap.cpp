/*
 * cmd_keymap.cpp
 *
 * 报文（参考工程 sendCurrentKeymapSnapshot / parseKeymapSetCommand）：
 *   0x05 响应：{"cmd":0x85,"seq":N,"status":0,"keymap":[
 *     {"physical":1,"normal":"a+b","macro":"","text":"hi","function":""}, ...]}
 *   0x06 请求：data.keymap = [{physical(1~11), normal?, macro?, text?, function?}]
 *   0x06 响应：通用成功（cmd|0x80）。
 *
 * 优先级：function > text（文本注入，ASCII，≤128 字符）> normal / macro。
 *
 * 键映射写入当前激活 Profile 的 keymap{N}.ini，成功后
 * MainTask::reloadKeymap() 刷新 KeyResolver 并推送键映射屏标签。
 */

#include "cmd_keymap.h"

#include <ArduinoJson.h>
#include <string.h>

#include "../../app/AppContext.h"
#include "../../config/Configuration.h"
#include "../../input/MatrixScanner.h" // kMatrixKeyCount
#include "../../logging/LogManager.h"
#include "../../utils/keymap_types.h"
#include "../CommandRegistry.h"
#include "../SerialProtocol.h"

namespace ekeys::protocol::commands
{

    namespace
    {

        /* text 字段（文本注入）长度上限；HID 键盘仅支持 ASCII */
        constexpr size_t kMaxTextLen = 128;

        /* 把 "a+b+c" 数组拼回 "+" 串（跳过空项）；容量模板复用 GET/SET */
        template <size_t N>
        String joinPlus(const std::array<String, N> &items)
        {
            String out;
            for (const auto &s : items)
            {
                if (s.isEmpty())
                {
                    continue;
                }
                if (out.length() > 0)
                {
                    out += '+';
                }
                out += s;
            }
            return out;
        }

        /* 把 "+" 串拆进数组，超过容量的项丢弃并告警 */
        template <size_t N>
        void splitPlus(const char *value, std::array<String, N> &out)
        {
            for (auto &s : out)
            {
                s = String();
            }
            if (value == nullptr || value[0] == '\0')
            {
                return;
            }
            const char *p = value;
            size_t idx = 0;
            while (p != nullptr && *p != '\0')
            {
                if (idx >= N)
                {
                    LOG_WARNING("KEYMAP", "too many entries, truncated");
                    break;
                }
                const char *sep = strchr(p, '+');
                const size_t len = (sep != nullptr)
                                       ? static_cast<size_t>(sep - p)
                                       : strlen(p);
                out[idx++] = String(p).substring(0, len);
                p = (sep != nullptr) ? sep + 1 : nullptr;
            }
        }

        int handleKeymapGet(int cmd, int seq, JsonObject /*data*/)
        {
            (void)cmd;

            Configuration::KeymapArray map{};
            if (!Configuration::instance().loadActiveProfileKeyMapping(map))
            {
                /*
                 * keymap{N}.ini 缺失/全空：设备运行时（KeyResolver）此时
                 * 用的是默认 a~k，这里上报同样的默认值，保证 App 看到的
                 * 与设备实际行为一致。
                 */
                LOG_WARNING("KEYMAP", "profile keymap unavailable, report defaults");
                keymapFillDefaults(map);
            }

            JsonDocument doc;
            doc["cmd"] = CMD_KEYMAP_GET | 0x80;
            doc["seq"] = seq;
            doc["status"] = 0;
            JsonArray arr = doc["keymap"].to<JsonArray>();
            for (uint8_t key_id = 1; key_id <= kMatrixKeyCount; ++key_id)
            {
                const KeyMapping &m = map[key_id];
                JsonObject key = arr.add<JsonObject>();
                key["physical"] = key_id;
                key["normal"] = joinPlus(m.normal_key);
                key["macro"] = joinPlus(m.macros_key);
                key["text"] = m.text_key;
                key["function"] = m.function_key;
            }
            SerialProtocol::instance().sendDocument(doc);
            return 0;
        }

        /* 解析单键映射写入 out[key_id] 并置位 mask；成功返回 0 */
        int parseSingleKeyMapping(JsonObject key, Configuration::KeymapArray &out,
                                  uint16_t &mask)
        {
            if (!key["physical"].is<int>())
            {
                return -1;
            }
            const int key_id = key["physical"].as<int>();
            if (key_id < 1 || key_id > kMatrixKeyCount)
            {
                LOG_WARNING("KEYMAP", "physical key out of range: %d", key_id);
                return -1;
            }

            KeyMapping m;
            m.valid = true;
            if (key["function"].is<const char *>())
            {
                m.function_key = key["function"].as<const char *>();
            }

            /* function_key 非空时优先，text / normal / macro 留空；
             * text 为文本注入串（ASCII，≤128 字符，超长截断） */
            if (m.function_key.isEmpty() && key["text"].is<const char *>())
            {
                m.text_key = key["text"].as<const char *>();
                if (m.text_key.length() > kMaxTextLen)
                {
                    m.text_key = m.text_key.substring(0, kMaxTextLen);
                    LOG_WARNING("KEYMAP", "text too long, truncated to %u",
                                static_cast<unsigned>(kMaxTextLen));
                }
            }
            if (m.function_key.isEmpty() && m.text_key.isEmpty() &&
                key["normal"].is<const char *>())
            {
                splitPlus<kKeyMappingNormalCount>(
                    key["normal"].as<const char *>(), m.normal_key);
            }
            if (m.function_key.isEmpty() && m.text_key.isEmpty() &&
                key["macro"].is<const char *>())
            {
                splitPlus<kKeyMappingMacrosCount>(
                    key["macro"].as<const char *>(), m.macros_key);
            }

            out[static_cast<uint8_t>(key_id)] = m;
            mask |= static_cast<uint16_t>(1u << key_id);
            return 0;
        }

        int handleKeymapSet(int cmd, int seq, JsonObject data)
        {
            JsonArray arr = data["keymap"].as<JsonArray>();
            if (arr.isNull())
            {
                SerialProtocol::instance().sendErrorResponse(
                    cmd, seq, "missing 'keymap' array");
                return -1;
            }

            /*
             * 先全部解析进内存，再一次批量落盘：
             * 逐键 saveKey 会对 keymap{N}.ini 做 N 次完整读/写，
             * SPIFFS 写入开销大，批量后仅一次。
             */
            Configuration::KeymapArray map{};
            uint16_t mask = 0;
            int ok_count = 0;
            for (JsonObject key : arr)
            {
                if (parseSingleKeyMapping(key, map, mask) == 0)
                {
                    ++ok_count;
                }
            }
            if (ok_count == 0)
            {
                SerialProtocol::instance().sendErrorResponse(
                    cmd, seq, "no valid key mapping");
                return -1;
            }

            if (!Configuration::instance().saveKeyMappings(map, mask))
            {
                LOG_ERROR("KEYMAP", "save %d mappings failed", ok_count);
                SerialProtocol::instance().sendErrorResponse(
                    cmd, seq, "save keymap failed");
                return -1;
            }

            LOG_INFO("KEYMAP", "saved %d mappings, reloading", ok_count);
            AppContext::instance().mainTask().reloadKeymap();

            JsonDocument resp;
            resp["cmd"] = cmd | 0x80;
            resp["seq"] = seq;
            resp["status"] = 0;
            (void)resp["data"].to<JsonObject>();
            SerialProtocol::instance().sendDocument(resp);
            return 0;
        }

    } // namespace

    void registerKeymapHandlers()
    {
        CommandRegistry::instance().registerHandler(CMD_KEYMAP_GET,
                                                    handleKeymapGet);
        CommandRegistry::instance().registerHandler(CMD_KEYMAP_SET,
                                                    handleKeymapSet);
        LOG_INFO("CMD", "cmd_keymap registered (0x%02X/0x%02X)",
                 CMD_KEYMAP_GET, CMD_KEYMAP_SET);
    }

    void unregisterKeymapHandlers()
    {
        CommandRegistry::instance().unregisterHandler(CMD_KEYMAP_GET);
        CommandRegistry::instance().unregisterHandler(CMD_KEYMAP_SET);
    }

} // namespace ekeys::protocol::commands
