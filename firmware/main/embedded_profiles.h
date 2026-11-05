// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>

/// One car profile embedded at build time (profiles/*.json, NUL-terminated).
struct EmbeddedProfile {
    const char *file_name;
    const char *start;
    const char *end; ///< one past the terminating NUL
};

extern const EmbeddedProfile kEmbeddedProfiles[];
extern const size_t kEmbeddedProfileCount;
