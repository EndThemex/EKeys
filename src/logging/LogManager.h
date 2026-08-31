/*
 * LogManager.h
 *
 * 统一的日志输出接口。
 *
 * 本期（阶段 01）只通过 UART0（Serial）打印。
 * 阶段 06 之后再扩展 SPIFFS / Web 等回调通道（FEATURE_DOC §15）。
 *
 * 用法：
 *
 *     LOG_INFO(TAG, "setup ok, mode=%d", mode);
 *     LOG_ERROR(TAG, "gfx->begin() failed: %s", reason);
 */

#ifndef EKEYS_LOGGING_LOG_MANAGER_H
#define EKEYS_LOGGING_LOG_MANAGER_H

#include <Arduino.h>

namespace ekeys
{

  enum class LogLevel : uint8_t
  {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
  };

  class LogManager
  {
  public:
    /*
     * 输出当前位置级别日志。
     * fmt 与 printf 兼容。
     */
    static void log(LogLevel level, const char *tag, const char *fmt, ...);

    /*
     * 注册输出回调（阶段 06 用于扩展 SPIFFS / Web 等通道）。
     * 本期占位，传 nullptr 关闭回调，仅保留 Serial 输出。
     */
    using Sink = void (*)(LogLevel level, const char *tag, const char *message);
    static void setSink(Sink sink);
  };

} // namespace ekeys

/*
 * ------------------------------------------------------------
 * 便捷宏
 * ------------------------------------------------------------
 */
#define LOG_DEBUG(tag, ...) ::ekeys::LogManager::log(::ekeys::LogLevel::Debug, tag, __VA_ARGS__)
#define LOG_INFO(tag, ...) ::ekeys::LogManager::log(::ekeys::LogLevel::Info, tag, __VA_ARGS__)
#define LOG_WARNING(tag, ...) ::ekeys::LogManager::log(::ekeys::LogLevel::Warning, tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) ::ekeys::LogManager::log(::ekeys::LogLevel::Error, tag, __VA_ARGS__)

#endif // EKEYS_LOGGING_LOG_MANAGER_H
