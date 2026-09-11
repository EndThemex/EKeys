/*
 * cmd_audio.cpp
 *
 * 见 cmd_audio.h。上传状态（begin → data×N → end/abort）由本文件
 * 静态状态机维护：handler 全部在 MainTask 上下文执行
 * （SerialProtocol::poll / TcpChannel::process 都在 MainTask::loop），
 * 无并发访问，File 句柄可跨请求保持打开。
 */

#include "cmd_audio.h"

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <mbedtls/base64.h>
#include <stdio.h>
#include <string.h>

#include "../../audio/AudioPad.h"
#include "../../logging/LogManager.h"
#include "../CommandRegistry.h"
#include "../SerialProtocol.h"

namespace ekeys::protocol::commands
{

    namespace
    {

        using ekeys::AudioPad;

        /* ---- 约束常量（App 端 / 文档同步维护） ---- */
        /* 名字基段上限（不含 '.' 和扩展名） */
        constexpr size_t kNameBaseMax = 20;
        /* 完整文件名上限：20 + '.' + 3 = 24 字符 */
        constexpr size_t kNameLenMax = AudioPad::kNameLenMax;
        /* 单文件上限 2MB（用户要求限制音频文件大小；begin/data 双重校验） */
        constexpr uint32_t kMaxAudioFileBytes = 2u * 1024u * 1024u;
        /* 上传后必须保留的剩余空间 headroom（防 SPIFFS 碎片写死） */
        constexpr uint32_t kFreeHeadroomBytes = 64u * 1024u;
        /* list 返回条数上限 */
        constexpr size_t kMaxListEntries = 64;
        /* 每块二进制大小；b64 后 1368 字符 < 2048 行缓冲 */
        constexpr size_t kAudioBlockBytes = 1024;
        /* b64 字段长度上限（1024B → 1368 字符，留余量拒异常大块） */
        constexpr size_t kMaxB64Len = 1400;
        /* 解码静态缓冲按上限推导（1400 字符 b64 → 最多 1054B） */
        constexpr size_t kDecodeBufSize = (kMaxB64Len / 4) * 3 + 4;

        bool validAudioName(const char *name)
        {
            if (name == nullptr)
            {
                return false;
            }
            const size_t len = strlen(name);
            if (len < 5 || len > kNameLenMax)
            {
                return false;
            }
            const size_t base_len = len - 4; /* ".mp3" / ".wav" */
            if (base_len > kNameBaseMax)
            {
                return false;
            }
            for (size_t i = 0; i < base_len; ++i)
            {
                const char c = name[i];
                const bool ok = (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') || c == '_';
                if (!ok)
                {
                    return false;
                }
            }
            return strcmp(name + base_len, ".mp3") == 0 ||
                   strcmp(name + base_len, ".wav") == 0;
        }

        uint32_t freeBytes()
        {
            return static_cast<uint32_t>(SPIFFS.totalBytes() - SPIFFS.usedBytes());
        }

        void buildPath(const char *name, const char *suffix,
                       char *out, size_t out_len)
        {
            snprintf(out, out_len, "/%s%s", name, suffix);
        }

        /* ---- 上传状态机（单流互斥，MainTask 单线程访问） ---- */
        struct UploadState
        {
            bool active = false;
            char name[kNameLenMax + 1]{};
            uint32_t size = 0;     /* begin 声明的总大小 */
            uint32_t received = 0; /* 已收字节数 */
            uint32_t next_index = 0;
            File file;
        };
        UploadState g_upload;

        void resetUpload_locked()
        {
            if (g_upload.file)
            {
                g_upload.file.close();
            }
            g_upload.active = false;
            g_upload.name[0] = '\0';
            g_upload.size = 0;
            g_upload.received = 0;
            g_upload.next_index = 0;
        }

        /* 启动时清理上次异常掉电/断连残留的 .part 文件 */
        void cleanupStalePartFiles()
        {
            File root = SPIFFS.open("/");
            if (!root || !root.isDirectory())
            {
                return;
            }
            File entry = root.openNextFile();
            while (entry)
            {
                const char *n = entry.name();
                const size_t len = strlen(n);
                /* arduino-esp32 2.x：File::name() 不含前导 '/' */
                if (len > 5 && strcmp(n + len - 5, ".part") == 0)
                {
                    char path[64];
                    snprintf(path, sizeof(path), "/%s", n);
                    LOG_WARNING("AUDIO", "remove stale part file %s", path);
                    SPIFFS.remove(path);
                }
                entry = root.openNextFile();
            }
        }

        /* ---- 0x16 op handlers ---- */

        void respondFileList(int cmd, int seq)
        {
            JsonDocument resp;
            resp["cmd"] = cmd | 0x80;
            resp["seq"] = seq;
            resp["status"] = 0;
            JsonObject data = resp["data"].to<JsonObject>();
            JsonArray files = data["files"].to<JsonArray>();

            File root = SPIFFS.open("/");
            if (root && root.isDirectory())
            {
                File entry = root.openNextFile();
                size_t count = 0;
                while (entry && count < kMaxListEntries)
                {
                    if (!entry.isDirectory())
                    {
                        const char *n = entry.name();
                        /* 白名单过滤：只报 mp3/wav，隐藏 .part / ini / 图标等 */
                        if (validAudioName(n))
                        {
                            JsonObject f = files.add<JsonObject>();
                            f["name"] = n;
                            f["size"] = static_cast<uint32_t>(entry.size());
                            ++count;
                        }
                    }
                    entry = root.openNextFile();
                }
            }

            data["total_bytes"] = static_cast<uint32_t>(SPIFFS.totalBytes());
            data["used_bytes"] = static_cast<uint32_t>(SPIFFS.usedBytes());
            data["free_bytes"] = freeBytes();
            SerialProtocol::instance().sendDocument(resp);
        }

        int handleFileList(int cmd, int seq, JsonObject /*data*/)
        {
            respondFileList(cmd, seq);
            return 0;
        }

        int handleFileBegin(int cmd, int seq, JsonObject data)
        {
            const char *err = nullptr;
            const char *name = data["name"] | "";
            const uint32_t size = data["size"] | 0u;

            if (!validAudioName(name))
            {
                err = "invalid file name";
            }
            else if (size == 0 || size > kMaxAudioFileBytes)
            {
                err = "size out of range (1B..2MB)";
            }
            else if (g_upload.active)
            {
                err = "another upload in progress";
            }
            else if (freeBytes() < size + kFreeHeadroomBytes)
            {
                err = "not enough free space";
            }
            if (err != nullptr)
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq, err);
                return -1;
            }

            /* 覆盖残留 .part（上次异常未 abort） */
            char part[64];
            buildPath(name, ".part", part, sizeof(part));
            if (SPIFFS.exists(part))
            {
                SPIFFS.remove(part);
            }

            g_upload.file = SPIFFS.open(part, FILE_WRITE);
            if (!g_upload.file)
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "create part file failed");
                return -1;
            }
            snprintf(g_upload.name, sizeof(g_upload.name), "%s", name);
            g_upload.size = size;
            g_upload.received = 0;
            g_upload.next_index = 0;
            g_upload.active = true;
            LOG_INFO("AUDIO", "upload begin %s (%u bytes)", part,
                     static_cast<unsigned>(size));

            JsonDocument resp;
            resp["cmd"] = cmd | 0x80;
            resp["seq"] = seq;
            resp["status"] = 0;
            JsonObject out = resp["data"].to<JsonObject>();
            out["received"] = 0;
            out["free_bytes"] = freeBytes();
            SerialProtocol::instance().sendDocument(resp);
            return 0;
        }

        int handleFileData(int cmd, int seq, JsonObject data)
        {
            const char *name = data["name"] | "";
            const uint32_t index = data["index"] | 0u;
            const char *b64 = data["b64"] | "";

            if (!g_upload.active || strcmp(name, g_upload.name) != 0)
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "no matching upload");
                return -1;
            }
            if (index != g_upload.next_index)
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "block index mismatch");
                return -1;
            }
            const size_t b64_len = strlen(b64);
            if (b64_len == 0 || b64_len > kMaxB64Len)
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "b64 length out of range");
                return -1;
            }

            /* 解码（静态缓冲按上限分配，避免每块堆分配） */
            static uint8_t decode_buf[kDecodeBufSize];
            size_t decoded = 0;
            const int rc = mbedtls_base64_decode(
                decode_buf, sizeof(decode_buf), &decoded,
                reinterpret_cast<const unsigned char *>(b64), b64_len);
            if (rc != 0 || decoded == 0)
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "b64 decode failed");
                return -1;
            }
            if (g_upload.received + decoded > g_upload.size)
            {
                /* 超声明大小：直接终止流（App 收到错误后会 abort 回滚） */
                resetUpload_locked();
                char part[64];
                buildPath(name, ".part", part, sizeof(part));
                SPIFFS.remove(part);
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "exceeds declared size");
                return -1;
            }
            if (!g_upload.file || g_upload.file.write(decode_buf, decoded) != decoded)
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "write failed");
                return -1;
            }

            g_upload.received += decoded;
            ++g_upload.next_index;

            JsonDocument resp;
            resp["cmd"] = cmd | 0x80;
            resp["seq"] = seq;
            resp["status"] = 0;
            JsonObject out = resp["data"].to<JsonObject>();
            out["received"] = g_upload.received;
            SerialProtocol::instance().sendDocument(resp);
            return 0;
        }

        int handleFileEnd(int cmd, int seq, JsonObject data)
        {
            const char *name = data["name"] | "";
            const uint32_t size = data["size"] | 0u;

            if (!g_upload.active || strcmp(name, g_upload.name) != 0 ||
                size != g_upload.size)
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "no matching upload");
                return -1;
            }
            if (g_upload.received != g_upload.size)
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "size mismatch");
                return -1;
            }

            g_upload.file.close();
            char part[64];
            buildPath(name, ".part", part, sizeof(part));
            char final_path[kNameLenMax + 2];
            buildPath(name, "", final_path, sizeof(final_path));

            /* SPIFFS rename 到已存在目标行为不确定 → 先删旧终名再原子改名 */
            if (SPIFFS.exists(final_path))
            {
                SPIFFS.remove(final_path);
            }
            if (!SPIFFS.rename(part, final_path))
            {
                /* 提交失败：保留 .part 供诊断，清流状态（App 可重传） */
                resetUpload_locked();
                LOG_ERROR("AUDIO", "rename %s failed", part);
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "commit failed");
                return -1;
            }

            resetUpload_locked();
            LOG_INFO("AUDIO", "upload committed %s (%u bytes)", final_path,
                     static_cast<unsigned>(size));

            JsonDocument resp;
            resp["cmd"] = cmd | 0x80;
            resp["seq"] = seq;
            resp["status"] = 0;
            JsonObject out = resp["data"].to<JsonObject>();
            out["free_bytes"] = freeBytes();
            SerialProtocol::instance().sendDocument(resp);
            return 0;
        }

        int handleFileAbort(int cmd, int seq, JsonObject data)
        {
            const char *name = data["name"] | "";
            if (g_upload.active && strcmp(name, g_upload.name) == 0)
            {
                resetUpload_locked();
            }
            /* 幂等：状态不匹配也回成功（App 超时重试 / 未知残留场景） */
            char part[64];
            buildPath(name, ".part", part, sizeof(part));
            if (validAudioName(name) && SPIFFS.exists(part))
            {
                SPIFFS.remove(part);
            }

            JsonDocument resp;
            resp["cmd"] = cmd | 0x80;
            resp["seq"] = seq;
            resp["status"] = 0;
            JsonObject out = resp["data"].to<JsonObject>();
            out["free_bytes"] = freeBytes();
            SerialProtocol::instance().sendDocument(resp);
            return 0;
        }

        int handleFileDelete(int cmd, int seq, JsonObject data)
        {
            const char *name = data["name"] | "";
            if (!validAudioName(name))
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "invalid file name");
                return -1;
            }
            if (AudioPad::instance().isPlayingFile(name))
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "file is playing");
                return -1;
            }
            char final_path[kNameLenMax + 2];
            buildPath(name, "", final_path, sizeof(final_path));
            if (!SPIFFS.exists(final_path))
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "no such file");
                return -1;
            }
            if (!SPIFFS.remove(final_path))
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "remove failed");
                return -1;
            }

            /* 先清绑定（内存），响应带回最新 pads，再落盘（同 0x15 先 ACK 模式） */
            const uint8_t cleared = AudioPad::instance().clearBindingsOf(name);
            if (cleared != 0)
            {
                LOG_INFO("AUDIO", "deleted %s, cleared %u binding(s)",
                         final_path, static_cast<unsigned>(cleared));
            }

            JsonDocument resp;
            resp["cmd"] = cmd | 0x80;
            resp["seq"] = seq;
            resp["status"] = 0;
            JsonObject out = resp["data"].to<JsonObject>();
            JsonArray pads = out["pads"].to<JsonArray>();
            char bindings[AudioPad::kPadCount][AudioPad::kNameLenMax + 1];
            AudioPad::instance().snapshotBindings(bindings);
            for (uint8_t k = 0; k < AudioPad::kPadCount; ++k)
            {
                JsonObject p = pads.add<JsonObject>();
                p["key"] = static_cast<uint8_t>(k + 1);
                p["file"] = bindings[k];
            }
            out["free_bytes"] = freeBytes();
            SerialProtocol::instance().sendDocument(resp);

            if (cleared != 0 && !AudioPad::instance().persist())
            {
                LOG_ERROR("AUDIO", "persist bindings after delete failed");
            }
            return 0;
        }

        int handleAudioFile(int cmd, int seq, JsonObject data)
        {
            const char *op = data["op"] | "";
            if (strcmp(op, "list") == 0)
            {
                return handleFileList(cmd, seq, data);
            }
            if (strcmp(op, "begin") == 0)
            {
                return handleFileBegin(cmd, seq, data);
            }
            if (strcmp(op, "data") == 0)
            {
                return handleFileData(cmd, seq, data);
            }
            if (strcmp(op, "end") == 0)
            {
                return handleFileEnd(cmd, seq, data);
            }
            if (strcmp(op, "abort") == 0)
            {
                return handleFileAbort(cmd, seq, data);
            }
            if (strcmp(op, "delete") == 0)
            {
                return handleFileDelete(cmd, seq, data);
            }
            SerialProtocol::instance().sendErrorResponse(cmd, seq, "unknown op");
            return -1;
        }

        /* ---- 0x17 op handlers ---- */

        void appendPads(JsonArray pads)
        {
            char bindings[AudioPad::kPadCount][AudioPad::kNameLenMax + 1];
            AudioPad::instance().snapshotBindings(bindings);
            for (uint8_t k = 0; k < AudioPad::kPadCount; ++k)
            {
                JsonObject p = pads.add<JsonObject>();
                p["key"] = static_cast<uint8_t>(k + 1);
                p["file"] = bindings[k];
            }
        }

        int handlePadGet(int cmd, int seq)
        {
            JsonDocument resp;
            resp["cmd"] = cmd | 0x80;
            resp["seq"] = seq;
            resp["status"] = 0;
            JsonObject data = resp["data"].to<JsonObject>();
            JsonArray pads = data["pads"].to<JsonArray>();
            appendPads(pads);
            SerialProtocol::instance().sendDocument(resp);
            return 0;
        }

        int handlePadSet(int cmd, int seq, JsonObject data)
        {
            const uint8_t key = data["key"] | 0u;
            const char *file = data["file"] | "";
            if (key < 1 || key > AudioPad::kPadCount)
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "key out of range");
                return -1;
            }
            /* file = "" 清除绑定；非空要求白名单 + 文件存在（setBinding 兜底再查） */
            if (!AudioPad::instance().setBinding(key, file))
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "set binding failed");
                return -1;
            }

            JsonDocument resp;
            resp["cmd"] = cmd | 0x80;
            resp["seq"] = seq;
            resp["status"] = 0;
            JsonObject out = resp["data"].to<JsonObject>();
            out["key"] = key;
            out["file"] = file;
            SerialProtocol::instance().sendDocument(resp);

            /* 先 ACK 再落盘（SPIFFS 全量写 ~1s，放 ACK 前会拖慢响应）
             * 失败仅记日志：内存已生效，下次 0x17 get 以设备实际为准。 */
            if (!AudioPad::instance().persist())
            {
                LOG_ERROR("AUDIO", "persist binding key%u failed",
                          static_cast<unsigned>(key));
            }
            return 0;
        }

        int handlePadPlay(int cmd, int seq, JsonObject data)
        {
            bool ok = false;
            if (data["key"].is<uint8_t>() || data["key"].is<int>())
            {
                ok = AudioPad::instance().trigger(data["key"].as<uint8_t>());
            }
            else if (data["file"].is<const char *>())
            {
                ok = AudioPad::instance().playFile(data["file"].as<const char *>());
            }
            if (!ok)
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "play rejected (unbound/missing/recording)");
                return -1;
            }
            JsonDocument resp;
            resp["cmd"] = cmd | 0x80;
            resp["seq"] = seq;
            resp["status"] = 0;
            JsonObject out = resp["data"].to<JsonObject>();
            out["playing"] = true;
            SerialProtocol::instance().sendDocument(resp);
            return 0;
        }

        int handlePadStop(int cmd, int seq)
        {
            AudioPad::instance().stop();
            JsonDocument resp;
            resp["cmd"] = cmd | 0x80;
            resp["seq"] = seq;
            resp["status"] = 0;
            JsonObject out = resp["data"].to<JsonObject>();
            out["playing"] = false;
            SerialProtocol::instance().sendDocument(resp);
            return 0;
        }

        int handleAudioPad(int cmd, int seq, JsonObject data)
        {
            const char *op = data["op"] | "";
            if (strcmp(op, "get") == 0)
            {
                return handlePadGet(cmd, seq);
            }
            if (strcmp(op, "set") == 0)
            {
                return handlePadSet(cmd, seq, data);
            }
            if (strcmp(op, "play") == 0)
            {
                return handlePadPlay(cmd, seq, data);
            }
            if (strcmp(op, "stop") == 0)
            {
                return handlePadStop(cmd, seq);
            }
            SerialProtocol::instance().sendErrorResponse(cmd, seq, "unknown op");
            return -1;
        }

    } // namespace

    void registerAudioHandlers()
    {
        cleanupStalePartFiles();
        CommandRegistry::instance().registerHandler(CMD_AUDIO_FILE, handleAudioFile);
        CommandRegistry::instance().registerHandler(CMD_AUDIO_PAD, handleAudioPad);
        LOG_INFO("CMD", "cmd_audio registered (0x%02X/0x%02X)",
                 CMD_AUDIO_FILE, CMD_AUDIO_PAD);
    }

    void unregisterAudioHandlers()
    {
        CommandRegistry::instance().unregisterHandler(CMD_AUDIO_FILE);
        CommandRegistry::instance().unregisterHandler(CMD_AUDIO_PAD);
    }

} // namespace ekeys::protocol::commands
