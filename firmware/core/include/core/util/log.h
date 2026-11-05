// SPDX-License-Identifier: GPL-3.0-or-later
//
// Minimal logging facade for core/. The platform installs a sink
// (ESP-IDF: esp_log, host: stderr or a test capture).
#pragma once

#include <cstdarg>

namespace core::log {

enum class Level { Error, Warn, Info, Debug };

using Sink = void (*)(Level level, const char *tag, const char *message);

/// Installs the sink. nullptr restores the default (discard).
void set_sink(Sink sink);

void write(Level level, const char *tag, const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;

} // namespace core::log

#define CORE_LOGE(tag, ...) ::core::log::write(::core::log::Level::Error, tag, __VA_ARGS__)
#define CORE_LOGW(tag, ...) ::core::log::write(::core::log::Level::Warn, tag, __VA_ARGS__)
#define CORE_LOGI(tag, ...) ::core::log::write(::core::log::Level::Info, tag, __VA_ARGS__)
#define CORE_LOGD(tag, ...) ::core::log::write(::core::log::Level::Debug, tag, __VA_ARGS__)
