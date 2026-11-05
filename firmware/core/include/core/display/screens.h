// SPDX-License-Identifier: GPL-3.0-or-later
//
// Screen navigation: one signal per screen, user order, per-screen style.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace core::display {

enum class GaugeStyle : uint8_t {
    Analog,  ///< OEM-like needle
    Digital, ///< racing: big number + bar
};

struct ScreenEntry {
    size_t slot;      ///< signal slot in the active profile
    GaugeStyle style; ///< per-screen style
};

class ScreenList {
public:
    void set(std::vector<ScreenEntry> screens);
    /// Default: every slot of the profile, in profile order, with one style.
    void set_default(size_t slot_count, GaugeStyle style);

    bool empty() const { return screens_.empty(); }
    size_t size() const { return screens_.size(); }
    size_t index() const { return index_; }
    const ScreenEntry &current() const { return screens_[index_]; }

    void next();
    void prev();
    /// Jumps to the screen showing `slot` (alarm auto-jump). False if no screen shows it.
    bool jump_to_slot(size_t slot);
    void toggle_style();

private:
    std::vector<ScreenEntry> screens_;
    size_t index_ = 0;
};

} // namespace core::display
