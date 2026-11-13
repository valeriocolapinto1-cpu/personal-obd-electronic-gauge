// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/display/screens.h"

namespace core::display {

void ScreenList::set(std::vector<ScreenEntry> screens)
{
    screens_ = std::move(screens);
    index_ = 0;
}

void ScreenList::set_default(size_t slot_count, GaugeStyle style)
{
    std::vector<ScreenEntry> v;
    for (size_t i = 0; i < slot_count; ++i) {
        v.push_back(ScreenEntry{i, style});
    }
    set(std::move(v));
}

void ScreenList::next()
{
    if (!screens_.empty()) {
        index_ = (index_ + 1) % screens_.size();
    }
}

void ScreenList::prev()
{
    if (!screens_.empty()) {
        index_ = (index_ + screens_.size() - 1) % screens_.size();
    }
}

bool ScreenList::jump_to_slot(size_t slot)
{
    for (size_t i = 0; i < screens_.size(); ++i) {
        if (screens_[i].slot == slot) {
            index_ = i;
            return true;
        }
    }
    return false;
}

void ScreenList::toggle_style()
{
    if (screens_.empty()) {
        return;
    }
    GaugeStyle &s = screens_[index_].style;
    s = (s == GaugeStyle::Analog) ? GaugeStyle::Digital : GaugeStyle::Analog;
}

} // namespace core::display
