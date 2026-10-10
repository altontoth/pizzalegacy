/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/commands/TutorialCommands.h"

#include <cstdint>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <string>
#include <variant>

#include "core/state/GameState.h"
#include "core/tutorial/Tutorial.h"
#include "core/tutorial/TutorialGame.h"
#include "core/tutorial/TutorialStep.h"
#include "engine/FormatCompat.h"

using namespace debug::command;

namespace debug::commands {

namespace {

std::string target_text(const core::tutorial::Step& step) {
  if (!step.target) {
    return "none";
  }
  if (const auto* item = std::get_if<std::string>(&*step.target)) {
    return compat::format("item {}", *item);
  }
  if (const auto* tile = std::get_if<core::tutorial::MapTile>(&*step.target)) {
    return compat::format("map tile {},{}", tile->x, tile->y);
  }
  return "the popup";
}

std::string filter_text(const core::tutorial::ClickFilter& filter) {
  if (!filter.active) {
    return "none";
  }
  std::string text = "target";
  for (const auto& prefix : filter.allowed_prefixes) {
    text += compat::format(", {}*", prefix);
  }
  return text;
}

CommandDef make_tutorial_start() {
  return CommandDef{
      .name = "start",
      .description = "Start a new game in the tutorial",
      .help_text = R"(Usage: tutorial start [seed]

Starts a new game the way New game > Tutorial does: the tutorial's seed,
character and city, one human player, and the tutorial on its first step.
A seed builds the world from that seed instead, to try one out.
)",
      .args =
          {
              ArgSpec{
                  .name = "seed",
                  .type = ArgType::Int,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "World seed (default: the tutorial's own)",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        std::optional<uint32_t> seed;
        if (ctx.has_arg(0)) {
          seed = static_cast<uint32_t>(ctx.int_arg(0));
        }
        core::tutorial::start_tutorial_game(ctx.state, ctx.context, seed);
        const auto city = ctx.state.active_city_id();
        if (auto* player = ctx.state.active_player()) {
          ctx.state.on_city_entered(city, *player);
        }
        return CommandResult::success().add_message(compat::format(
            "Tutorial started in city {}, on step {}", city.value,
            ctx.state.tutorial().step_index().value_or(0)));
      },
  };
}

CommandDef make_tutorial_status() {
  return CommandDef{
      .name = "status",
      .description = "Show the tutorial's current step",
      .help_text = "Usage: tutorial status",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        const auto& tutorial = ctx.state.tutorial();
        const auto* step = tutorial.current_step();
        if (!step) {
          return CommandResult::success().add_message("No tutorial running");
        }
        auto result = CommandResult::success();
        result.add_message(compat::format("Step {} of {}: {}",
                                          *tutorial.step_index() + 1,
                                          tutorial.step_count(), step->name));
        result.add_message(compat::format(
            "Finishes on: {}", magic_enum::enum_name(step->completion)));
        result.add_message(compat::format("Target: {}", target_text(*step)));
        result.add_message(
            compat::format("ClickFilter: {}", filter_text(step->click_filter)));
        result.add_message(compat::format("Text: {}", step->text));
        return result;
      },
  };
}

CommandDef make_tutorial_next() {
  return CommandDef{
      .name = "next",
      .description = "Finish the current step as if the player had",
      .help_text = R"(Usage: tutorial next

Moves on from the current step whatever it waits for. A scripted run has no
panel to click, so this is how it gets past a step that waits for a click.
)",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto& tutorial = ctx.state.tutorial();
        if (!tutorial.active()) {
          return CommandResult::error("No tutorial running");
        }
        tutorial.advance();
        if (!tutorial.active()) {
          return CommandResult::success().add_message("Tutorial finished");
        }
        return CommandResult::success().add_message(
            compat::format("Now on step {}: {}", *tutorial.step_index() + 1,
                           tutorial.current_step()->name));
      },
  };
}

CommandDef make_tutorial_skip() {
  return CommandDef{
      .name = "skip",
      .description = "Leave the tutorial and play on",
      .help_text = "Usage: tutorial skip",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto& tutorial = ctx.state.tutorial();
        if (!tutorial.active()) {
          return CommandResult::error("No tutorial running");
        }
        tutorial.finish();
        return CommandResult::success().add_message("Tutorial left");
      },
  };
}

} // namespace

CommandDef make_tutorial_commands() {
  return CommandDef{
      .name = "tutorial",
      .description = "Tutorial commands",
      .help_text = "Start and walk the tutorial (start, status, next, skip)",
  }
      .add_subcommand(make_tutorial_start())
      .add_subcommand(make_tutorial_status())
      .add_subcommand(make_tutorial_next())
      .add_subcommand(make_tutorial_skip());
}

} // namespace debug::commands
