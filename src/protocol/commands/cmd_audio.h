/*
 * cmd_audio.h
 *
 * 音效板协议（0x16 / 0x17），data.op 分发：
 *
 * CMD_AUDIO_FILE（0x16，文件管理）：
 *   op=list   请求 {}            → files:[{name,size}](≤64)、
 *                                  total_bytes/used_bytes/free_bytes
 *   op=begin  {name,size}        → {received:0, free_bytes}；建 /name.part
 *   op=data   {name,index,b64}   → {received:N}；每块 1024B 二进制
 *                                  （b64 后 ~1.4KB < 2048 行缓冲）
 *   op=end    {name,size}        → 校验一致 → .part 原子改名提交 →
 *                                  {free_bytes}
 *   op=abort  {name}             → 删 .part（App 取消 / 失败回滚）
 *   op=delete {name}             → 删终名 + 清绑定表中引用 →
 *                                  {pads:[{key,file}], free_bytes}
 *
 * CMD_AUDIO_PAD（0x17，绑定与播放）：
 *   op=get    {}                 → pads:[{key,file}]（11 键全量）
 *   op=set    {key,file}         → {key,file}（file="" 清除）；先 ACK 再落盘
 *   op=play   {key} 或 {file}    → 立即 ACK（试播不亮键位高亮）
 *   op=stop   {}                 → ACK
 *
 * 约束：文件名 ^[a-z0-9_]{1,20}\.(mp3|wav)$、SPIFFS 根目录、
 *       单文件 ≤ 2MB、上传 free ≥ size + 64KB headroom、单上传流互斥。
 *
 * 扩展预留：网络音频播放（Speaker::PlayRemoteAudio 已具备）后续在
 * 0x17 增加 op 或 play.url 字段，不动 0x16 文件管理语义。
 */

#ifndef EKEYS_PROTOCOL_COMMANDS_CMD_AUDIO_H
#define EKEYS_PROTOCOL_COMMANDS_CMD_AUDIO_H

namespace ekeys::protocol::commands
{

    void registerAudioHandlers();
    void unregisterAudioHandlers();

} // namespace ekeys::protocol::commands

#endif // EKEYS_PROTOCOL_COMMANDS_CMD_AUDIO_H
