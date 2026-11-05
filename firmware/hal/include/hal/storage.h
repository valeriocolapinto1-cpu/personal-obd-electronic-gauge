// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace hal {

/// Removable storage (microSD) used for logs and recorded bus traces.
class IRemovableStorage {
public:
    virtual ~IRemovableStorage() = default;
    virtual bool mount() = 0;
    virtual bool is_mounted() const = 0;
    /// Capacity in MiB, 0 when not mounted.
    virtual uint32_t capacity_mib() const = 0;
    /// VFS mount point, e.g. "/sdcard".
    virtual const char *mount_point() const = 0;
};

} // namespace hal
