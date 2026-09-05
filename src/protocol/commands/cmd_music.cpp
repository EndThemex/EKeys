/*
 * cmd_music.cpp
 *
 * 报文（参考工程 onMusicStatusCommand）：
 *   请求：{"cmd":0x0e,"seq":N,"data":{"music_status":{
 *     connected,is_playing,is_paused,can_prev,can_next,
 *     position_ms,duration_ms,title,artist,player,
 *     lyric_current,lyric_next }}}
 *   响应：{"cmd":0x8e,"seq":N,"status":0}
 */

#include "cmd_music.h"

#include <ArduinoJson.h>
#include <string.h>

#include "../../logging/LogManager.h"
#include "../../message_types.h"
#include "../SerialProtocol.h"
#include "../../tasks/DisplayTask.h"
#include "../CommandRegistry.h"

namespace ekeys::protocol::commands
{

    namespace
    {

        /* UTF-8 安全截断（不切半个多字节字符） */
        void copyUtf8Truncated(char *dst, size_t cap, const char *src)
        {
            if (cap == 0)
            {
                return;
            }
            size_t used = 0;
            size_t i = 0;
            while (src[i] != '\0')
            {
                const unsigned char c = static_cast<unsigned char>(src[i]);
                size_t full = 1;
                if (c >= 0xF0)
                    full = 4;
                else if (c >= 0xE0)
                    full = 3;
                else if (c >= 0xC0)
                    full = 2;
                if (used + full > cap - 1)
                {
                    break;
                }
                for (size_t k = 0; k < full && src[i + k] != '\0'; ++k, ++i)
                {
                    dst[used++] = src[i];
                }
            }
            dst[used] = '\0';
        }

        int handleMusicStatus(int cmd, int seq, JsonObject data)
        {
            JsonObject music = data["music_status"].as<JsonObject>();
            if (music.isNull())
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "missing 'music_status'");
                return -1;
            }

            DisplayMessage msg;
            msg.type = DisplayMessageType::MusicPlayer;

            MusicPlayerInfo &p = msg.music_player;
            p.connected = music["connected"] | false;
            p.is_playing = music["is_playing"] | false;
            p.is_paused = music["is_paused"] | false;
            p.can_prev = music["can_prev"] | false;
            p.can_next = music["can_next"] | false;
            p.current_seconds = static_cast<uint16_t>((music["position_ms"] | 0) / 1000);
            p.total_seconds = static_cast<uint16_t>((music["duration_ms"] | 0) / 1000);

            copyUtf8Truncated(p.title, sizeof(p.title),
                              music["title"] | "WAITING FOR PLAYER");
            copyUtf8Truncated(p.artist, sizeof(p.artist), music["artist"] | "");
            copyUtf8Truncated(p.player_name, sizeof(p.player_name),
                              music["player"] | "PC MUSIC");
            copyUtf8Truncated(p.lyric_current, sizeof(p.lyric_current),
                              music["lyric_current"] | "");
            copyUtf8Truncated(p.lyric_next, sizeof(p.lyric_next),
                              music["lyric_next"] | "");

            DisplayTask::instance().post(msg, 0);
            SerialProtocol::instance().sendSuccessResponse(cmd, seq,
                                                           JsonObject());
            return 0;
        }

    } // namespace

    void registerMusicHandlers()
    {
        CommandRegistry::instance().registerHandler(
            CMD_MUSIC_STATUS, handleMusicStatus);
        LOG_INFO("CMD", "cmd_music registered (0x%02X)", CMD_MUSIC_STATUS);
    }

    void unregisterMusicHandlers()
    {
        CommandRegistry::instance().unregisterHandler(CMD_MUSIC_STATUS);
    }

} // namespace ekeys::protocol::commands
