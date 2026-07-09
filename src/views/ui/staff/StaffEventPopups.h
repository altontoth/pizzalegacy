/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>

#include "core/restaurant/StaffEventData.h"

class GameContext;
class Restaurant;

namespace views::ui::staff {

// Drive the staff event dialog chain: excuse AlertPopup -> OptionsPopup ->
// optional reply AlertPopup. Outcome effects are applied through
// StaffEvents::apply_outcome; this only presents. Registered on GameState as
// the staff event presenter so core code can request it without depending on
// popup types.
void show_staff_event_dialog(GameContext& context,
                             const core::restaurant::StaffEventData& event,
                             Restaurant* restaurant,
                             std::function<void()> on_done);

} // namespace views::ui::staff
