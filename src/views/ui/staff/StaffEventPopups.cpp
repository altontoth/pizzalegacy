/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/staff/StaffEventPopups.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/character/Staff.h"
#include "core/factories/CharacterFactory.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/StaffEventDialogs.h"
#include "core/restaurant/StaffEvents.h"
#include "core/util/GameContext.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/geom/Spaces.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/AlertPopup.h"
#include "views/ui/popups/OptionsPopup.h"

namespace views::ui::staff {

namespace {
constexpr ViewPoint dialog_origin{28, 40};
} // namespace

void show_staff_event_dialog(GameContext& context,
                             const core::restaurant::StaffEventData& event,
                             Restaurant* restaurant,
                             std::function<void()> on_done) {
  using core::restaurant::StaffEventDialogs;
  using core::restaurant::StaffEvents;

  Staff* initial_staff = restaurant->staff_at(event.job_role, event.slot_index);
  Logger::debug(
      "[staff_event] show_dialog: rest={} role={} slot={} type={} staff={}",
      restaurant->id().value, static_cast<int>(event.job_role),
      event.slot_index, static_cast<int>(event.type),
      initial_staff
          ? compat::format("{} {}", initial_staff->character().first_name(),
                           initial_staff->character().last_name())
          : "(none)");

  auto texts = StaffEventDialogs::pick_dialog_texts(event.type);
  if (!texts) {
    on_done();
    return;
  }

  std::string excuse_text =
      compat::format("{}: {}", restaurant->name(), texts->excuse);

  const RawRenderable* staff_pic = nullptr;
  if (initial_staff) {
    // Get the picture from the factory because the Staff object can go
    // away if they get fired. Technically I think we could hire a new character
    // in that same slot, but I don't care enough to special case that.
    staff_pic =
        CharacterFactory::create_character(initial_staff->character().id());
  }

  // Callback that applies the game effect and shows the optional reply popup.
  // Using shared_ptr so it can be safely shared between three option
  // lambdas.
  auto handle = std::make_shared<std::function<void(int)>>(
      [ctx = &context, event, restaurant, staff_pic,
       on_done = std::move(on_done)](int outcome) {
        auto result = StaffEvents::apply_outcome(event, outcome, *restaurant);
        if (!result || !result->reply) {
          on_done();
          return;
        }
        // Quit replies keep the default centered popup origin.
        if (event.type != core::restaurant::StaffEventData::Type::Quit) {
          ctx->popup->origin(dialog_origin);
        }
        ctx->popup->create<AlertPopup>(std::move(*result->reply), on_done,
                                       staff_pic);
      });

  // Show excuse, then the options popup when dismissed.
  context.popup->origin(dialog_origin);
  context.popup->create<AlertPopup>(
      std::move(excuse_text),
      [ctx = &context, options = std::move(texts->options), handle]() {
        std::vector<Option> option_items{
            {.text = options[0], .callback = [handle]() { (*handle)(0); }},
            {.text = options[1], .callback = [handle]() { (*handle)(1); }},
            {.text = options[2], .callback = [handle]() { (*handle)(2); }},
        };
        ctx->popup->origin(dialog_origin);
        ctx->popup->create<OptionsPopup>(std::move(option_items));
      },
      staff_pic);
}

} // namespace views::ui::staff
