// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/util/log.h"

#include <atomic>
#include <cstdio>

namespace core::log {

namespace {
std::atomic<Sink> g_sink{nullptr};
}

void set_sink(Sink sink)
{
    g_sink.store(sink);
}

void write(Level level, const char *tag, const char *fmt, ...)
{
    Sink sink = g_sink.load();
    if (sink == nullptr) {
        return;
    }
    char buf[192];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    sink(level, tag, buf);
}

} // namespace core::log
