/*
 * cmd_keymap.cpp
 *
 * 报文（参考工程 sendCurrentKeymapSnapshot / parseKeymapSetCommand）：
 *   0x05 响应：{"cmd":0x85,"seq":N,"status":0,"fun_key1":1,"fun_key2":0,
 *     "keymap":[{"physical":1,"normal":"a+b","macro":"","text":"hi","function":"",
 *       "combo1_normal":"Ctrl+c","combo1_text":"","combo1_function":"",
 *       "combo2_normal":"","combo2_text":"","combo2_function":""}, ...]}
 *   0x06 请求：data.fun_key1 / data.fun_key2（可选，0~11，出现时持久化），
 *     data.keymap = [{physical(1~11), normal?, macro?, text?, function?,
 *       combo1_normal?, combo1_text?, combo1_function?,
 *       combo2_normal?, combo2_text?, combo2_function?}]
 *   0x06 响应：通用成功（cmd|0x80）。
 *
 * 优先级：单击 function > text（文本注入，ASCII，≤128 字符）> normal / macro；
 * 组合层 combo1（FUN1 按住）/ combo2（FUN2 按住）内部优先级同上，
 * 与单击通道互相独立（运行时按 FUN 键是否按住选择，FUN1 层优先于 FUN2）。
 *
 * 键映射写入当前激活 Profile 的 keymap{N}.ini，fun_key 持久化到
 * config.ini [system]。处理顺序：先更新运行时（内存设置 +
 * MainTask::applyKeymap 直刷 KeyResolver）并回 ACK，再落盘 SPIFFS
 * （atomic 写耗时秒级，放 ACK 前会拖慢响应导致 App 超时误判）。
 */

#include "cmd_keymap.h"

#include <ArduinoJson.h>
#include <string.h>

#include <initializer_list>
#include <utility>

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

        /* 解析一层组合通道（layerPrefix 为 "combo1"/"combo2"）并写入 m；
         * 层内优先级 function > text > normal，高优先级非空时低优先级留空 */
        void parseComboLayer(JsonObject key, const char *layerPrefix,
                             KeyMapping &m)
        {
            String &fk = (layerPrefix[5] == '1') ? m.combo1_function_key
                                                 : m.combo2_function_key;
            String &tk = (layerPrefix[5] == '1') ? m.combo1_text_key
                                                 : m.combo2_text_key;
            std::array<String, kKeyMappingNormalCount> &nk =
                (layerPrefix[5] == '1') ? m.combo1_normal_key
                                        : m.combo2_normal_key;

            char field[24];
            snprintf(field, sizeof(field), "%s_function", layerPrefix);
            if (key[field].is<const char *>())
            {
                fk = key[field].as<const char *>();
            }
            if (fk.isEmpty())
            {
                snprintf(field, sizeof(field), "%s_text", layerPrefix);
                if (key[field].is<const char *>())
                {
                    tk = key[field].as<const char *>();
                    if (tk.length() > kMaxTextLen)
                    {
                        tk = tk.substring(0, kMaxTextLen);
                        LOG_WARNING("KEYMAP", "%s too long, truncated to %u",
                                    field, static_cast<unsigned>(kMaxTextLen));
                    }
                }
            }
            if (fk.isEmpty() && tk.isEmpty())
            {
                snprintf(field, sizeof(field), "%s_normal", layerPrefix);
                if (key[field].is<const char *>())
                {
                    splitPlus<kKeyMappingNormalCount>(
                        key[field].as<const char *>(), nk);
                }
            }
        }

        int handleKeymapGet(int cmd, int seq, JsonObject /*data*/)
        {
            (void)cmd;

            /*
             * KeymapArray 约 4.3KB（12 键 × KeyMapping，含 combo 通道），
             * 命令在 loopTask（8KB 栈）里执行，栈上声明会溢出复位，
             * 必须放静态存储；命令处理为 MainTask 单线程，无并发问题。
             */
            static Configuration::KeymapArray map{};
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
            doc["fun_key1"] = Configuration::instance().settings().fun_key1;
            doc["fun_key2"] = Configuration::instance().settings().fun_key2;
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
                key["combo1_normal"] = joinPlus(m.combo1_normal_key);
                key["combo1_text"] = m.combo1_text_key;
                key["combo1_function"] = m.combo1_function_key;
                key["combo2_normal"] = joinPlus(m.combo2_normal_key);
                key["combo2_text"] = m.combo2_text_key;
                key["combo2_function"] = m.combo2_function_key;
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

            /* FUN 组合层：与单击通道互相独立（运行时按 FUN 键是否按住选择） */
            parseComboLayer(key, "combo1", m);
            parseComboLayer(key, "combo2", m);

            out[static_cast<uint8_t>(key_id)] = m;
            mask |= static_cast<uint16_t>(1u << key_id);
            return 0;
        }

        /*
         * 解析可选的 data.fun_key1 / data.fun_key2（FUN 组合键，0~11，0=未配置），
         * 合法时更新内存设置（持久化由调用方在保存成功后执行）。
         * 未出现的字段保持现值。成功返回 0，非法返回 -1。
         */
        int parseFunKeys(JsonObject data, uint8_t &out_fk1, uint8_t &out_fk2)
        {
            Configuration &config = Configuration::instance();
            DeviceSettings snap;
            config.snapshot(snap);
            out_fk1 = snap.fun_key1;
            out_fk2 = snap.fun_key2;

            if (data["fun_key1"].is<int>())
            {
                const int v = data["fun_key1"].as<int>();
                if (v < 0 || v > kMatrixKeyCount)
                {
                    LOG_WARNING("KEYMAP", "fun_key1 %d out of range", v);
                    return -1;
                }
                out_fk1 = static_cast<uint8_t>(v);
            }
            if (data["fun_key2"].is<int>())
            {
                const int v = data["fun_key2"].as<int>();
                if (v < 0 || v > kMatrixKeyCount)
                {
                    LOG_WARNING("KEYMAP", "fun_key2 %d out of range", v);
                    return -1;
                }
                out_fk2 = static_cast<uint8_t>(v);
            }
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

            /* 可选 fun_key1 / fun_key2 先校验，非法直接拒绝本次写入 */
            uint8_t fk1 = 0;
            uint8_t fk2 = 0;
            if (parseFunKeys(data, fk1, fk2) != 0)
            {
                SerialProtocol::instance().sendErrorResponse(
                    cmd, seq, "fun_key out of range (0~11)");
                return -1;
            }

            /*
             * 先全部解析进内存，再一次批量落盘：
             * 逐键 saveKey 会对 keymap{N}.ini 做 N 次完整读/写，
             * SPIFFS 写入开销大，批量后仅一次。
             */
            /* 同 handleKeymapGet：KeymapArray 过大，放静态存储避免栈溢出 */
            static Configuration::KeymapArray map{};
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

            Configuration &config = Configuration::instance();

            /*
             * 性能优化：先更新运行时（内存设置 + KeyResolver）并立即回 ACK，
             * SPIFFS 持久化放在 ACK 之后执行。0x06 的 SPIFFS atomic 写
             * （keymap{N}.ini + config.ini）实测合计约 4s，放在 ACK 前
             * 会导致 App 端 3s 超时误判下发失败（响应迟到被当未配对帧丢弃）。
             * ACK 后落盘失败仅记 ERROR 日志：运行时已是新映射，下次 0x05
             * 回读以设备实际为准，掉电窗口内最多丢失本次写入（回退旧配置）。
             */

            /* FUN 组合键更新内存（KeyResolver 每次按键实时读取）；
             * 与「字段存在性=显式配置」语义一致 */
            const bool fk1_present = data["fun_key1"].is<int>();
            const bool fk2_present = data["fun_key2"].is<int>();
            if (fk1_present || fk2_present)
            {
                (void)config.mutateSettings([&](DeviceSettings &d)
                                            {
                    if (fk1_present)
                    {
                        d.fun_key1 = fk1;
                    }
                    if (fk2_present)
                    {
                        d.fun_key2 = fk2;
                    } });
            }

            /* 内存映射直刷 KeyResolver（免 SPIFFS 重读）+ 键映射屏标签更新 */
            AppContext::instance().mainTask().applyKeymap(map, mask);

            JsonDocument resp;
            resp["cmd"] = cmd | 0x80;
            resp["seq"] = seq;
            resp["status"] = 0;
            (void)resp["data"].to<JsonObject>();
            SerialProtocol::instance().sendDocument(resp);

            /* ---- ACK 已发出，以下为持久化（阻塞 MainTask 但不再拖慢响应） ---- */
            if (!config.saveKeyMappings(map, mask))
            {
                LOG_ERROR("KEYMAP", "save %d mappings failed", ok_count);
            }

            if (fk1_present || fk2_present)
            {
                /* 单次 config.ini 读/写覆盖全部出现的 fun_key（批量接口）；
                 * saveSetting 内部自行加锁，不能放进 mutator */
                std::initializer_list<std::pair<const char *, int>> fk_kvs;
                if (fk1_present && fk2_present)
                {
                    fk_kvs = {{"fun_key1", fk1}, {"fun_key2", fk2}};
                }
                else if (fk1_present)
                {
                    fk_kvs = {{"fun_key1", fk1}};
                }
                else
                {
                    fk_kvs = {{"fun_key2", fk2}};
                }
                if (!config.saveSettings(fk_kvs))
                {
                    LOG_ERROR("KEYMAP", "persist fun_key failed");
                }
            }

            LOG_INFO("KEYMAP", "persisted %d mappings", ok_count);
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
