/*
 * LogManager.cpp
 *
 * 阶段 01：仅输出到 Serial。
 */

#include "LogManager.h"

#include <Arduino.h>
#include <stdarg.h>

namespace ekeys {

namespace {

constexpr char kTagUnknown[] = "?";

const char *levelPrefix(LogLevel level)
{
    switch (level) {
        case LogLevel::Debug:   return "D";
        case LogLevel::Info:    return "I";
        case LogLevel::Warning: return "W";
        case LogLevel::Error:   return "E";
    }
    return "?";
}

LogManager::Sink g_sink = nullptr;

}  // namespace

void LogManager::log(LogLevel level, const char *tag, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt ? fmt : "", ap);
    va_end(ap);
    if (n < 0) {
        buf[0] = '\0';
    }

    Serial.print('[');
    Serial.print(levelPrefix(level));
    Serial.print(']');
    Serial.print(tag ? tag : kTagUnknown);
    Serial.print(": ");
    Serial.println(buf);

    if (g_sink) {
        g_sink(level, tag, buf);
    }
}

void LogManager::setSink(Sink sink)
{
    g_sink = sink;
}

}  // namespace ekeys
