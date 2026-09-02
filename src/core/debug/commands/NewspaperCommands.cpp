/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "NewspaperCommands.h"
#include "core/city/City.h"
#include "core/city/IngredientTrendSystem.h"
#include "core/food/Ingredient.h"
#include "core/newspaper/NewspaperEventSystem.h"
#include "core/newspaper/NewspaperState.h"
#include "core/state/GameState.h"
#include "engine/FormatCompat.h"

using namespace debug::command;
using core::food::IngredientType;
using core::newspaper::NewspaperEvent;
using core::newspaper::NewspaperState;

namespace debug::commands {

// Schedule subcommands

static CommandDef make_news_schedule_list() {
  return CommandDef{
      .name = "list",
      .description = "Show scheduled event slots",
      .help_text = R"(Usage: news schedule list [all]

Shows non-empty scheduled event slots, or all 50 if "all" is given.

Examples:
  news schedule list          # Only non-empty slots
  news schedule list all      # All 50 slots
)",
      .args =
          {
              ArgSpec{
                  .name = "all",
                  .type = ArgType::String,
                  .optional = true,
                  .default_provider = DefaultProvider::EmptyString,
                  .description = "Pass 'all' to show all 50 slots",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }
        auto& sys = city->newspaper_event_system();
        auto& types = sys.scheduled_types();
        auto& countdowns = sys.scheduled_countdowns();
        bool show_all = !ctx.args.empty() && ctx.string_arg(0) == "all";

        std::vector<std::string> lines;
        lines.push_back("Scheduled event slots:");

        int shown = 0;
        for (int i = 0; i < 50; ++i) {
          auto event_id = static_cast<uint8_t>(types[i]);
          auto countdown = countdowns[i];
          if (!show_all && event_id == 0 && countdown == 0) {
            continue;
          }
          lines.push_back(
              compat::format("  [{:2}] event={} ({}) countdown={}", i, event_id,
                             core::newspaper::to_string(types[i]), countdown));
          ++shown;
        }

        if (shown == 0) {
          lines.push_back("  (no scheduled events)");
        }

        return CommandResult::success(lines);
      },
  };
}

static CommandDef make_news_schedule_set() {
  return CommandDef{
      .name = "set",
      .description = "Schedule an event into a slot",
      .help_text = R"(Usage: news schedule set <slot> <event_id> <days>

Schedules event_id (1-51) into a slot (0-49) to fire in <days> days.

Examples:
  news schedule set 0 1 5     # TomatoesContaminated in 5 days
  news schedule set 5 6 10    # HeatWave in 10 days
)",
      .args =
          {
              ArgSpec::required("slot", ArgType::Int, "Slot index (0-49)"),
              ArgSpec::required("event_id", ArgType::Int, "Event ID (1-51)"),
              ArgSpec::required("days", ArgType::Int, "Days until event fires"),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto slot = ctx.int_arg(0);
        auto event_id = ctx.int_arg(1);
        auto days = ctx.int_arg(2);

        if (slot < 0 || slot >= 50) {
          return CommandResult::error("Slot must be 0-49");
        }
        if (event_id < 1 || event_id > 51) {
          return CommandResult::error("Event ID must be 1-51");
        }
        if (days < 1 || days > 255) {
          return CommandResult::error("Days must be 1-255");
        }

        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }
        auto event = static_cast<NewspaperEvent>(event_id);
        city->newspaper_event_system().schedule_event(
            static_cast<int>(slot), event, static_cast<int>(days));

        return CommandResult::success().add_message(compat::format(
            "Scheduled {} ({}) in slot {} to fire in {} days",
            core::newspaper::to_string(event), event_id, slot, days));
      },
  };
}

static CommandDef make_news_schedule_clear() {
  return CommandDef{
      .name = "clear",
      .description = "Clear one or all scheduled slots",
      .help_text = R"(Usage: news schedule clear [slot]

Clears a specific slot (0-49) or all slots if no argument given.

Examples:
  news schedule clear         # Clear all slots
  news schedule clear 0       # Clear slot 0 only
)",
      .args =
          {
              ArgSpec{
                  .name = "slot",
                  .type = ArgType::Int,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "Slot index (0-49), or omit for all",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }
        auto& sys = city->newspaper_event_system();

        if (!ctx.args.empty()) {
          auto slot = ctx.int_arg(0);
          if (slot < 0 || slot >= 50) {
            return CommandResult::error("Slot must be 0-49");
          }
          sys.schedule_event(static_cast<int>(slot),
                             static_cast<NewspaperEvent>(0), 0);
          return CommandResult::success().add_message(
              compat::format("Cleared slot {}", slot));
        }

        for (int i = 0; i < 50; ++i) {
          sys.schedule_event(i, static_cast<NewspaperEvent>(0), 0);
        }
        return CommandResult::success().add_message(
            "Cleared all 50 scheduled slots");
      },
  };
}

static CommandDef make_news_schedule_fire() {
  return CommandDef{
      .name = "fire",
      .description = "Immediately execute a named event",
      .help_text = R"(Usage: news schedule fire <event_id>

Fires the event immediately, setting the article and applying effects.

Examples:
  news schedule fire 1        # Fire TomatoesContaminated
  news schedule fire 6        # Fire HeatWave
)",
      .args =
          {
              ArgSpec::required("event_id", ArgType::Int, "Event ID (1-51)"),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto event_id = ctx.int_arg(0);
        if (event_id < 1 || event_id > 51) {
          return CommandResult::error("Event ID must be 1-51");
        }

        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }

        auto event = static_cast<NewspaperEvent>(event_id);
        bool fired =
            city->newspaper_event_system().fire_event(event, ctx.state, *city);

        if (fired) {
          return CommandResult::success().add_message(compat::format(
              "Fired {} ({})", core::newspaper::to_string(event), event_id));
        }
        return CommandResult::error(
            compat::format("{} ({}) was suppressed (e.g. wrong season)",
                           core::newspaper::to_string(event), event_id));
      },
  };
}

static CommandDef make_news_schedule_generate() {
  return CommandDef{
      .name = "generate",
      .aliases = {"gen"},
      .description = "Generate a random ingredient trend event",
      .help_text = R"(Usage: news schedule generate

Rolls a random ingredient, checks if it's valid and not already trending,
then applies a random trend effect and sets the article state.
May fail if the random ingredient is invalid or already trending.

Examples:
  news schedule generate      # Try to generate a random ingredient event
)",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }

        bool generated =
            city->newspaper_event_system().generate_ingredient_event(ctx.state,
                                                                     *city);

        if (generated) {
          auto& ns = city->newspaper_state();
          auto name =
              ns.target_ingredient
                  ? std::string(core::food::to_string(*ns.target_ingredient))
                  : std::string("(unknown)");
          return CommandResult::success().add_message(compat::format(
              "Generated ingredient event: {} polarity={} category={}", name,
              ns.polarity, ns.category));
        }
        return CommandResult::error(
            "Generation failed (random ingredient invalid or already "
            "trending). Try again.");
      },
  };
}

static CommandDef make_news_schedule() {
  return CommandDef{
      .name = "schedule",
      .description = "Manage scheduled newspaper events",
      .help_text = "Subcommands: list, set, clear, fire, generate",
  }
      .add_subcommand(make_news_schedule_list())
      .add_subcommand(make_news_schedule_set())
      .add_subcommand(make_news_schedule_clear())
      .add_subcommand(make_news_schedule_fire())
      .add_subcommand(make_news_schedule_generate());
}

// Article subcommands

static CommandDef make_news_article_show() {
  return CommandDef{
      .name = "show",
      .description = "Dump current NewspaperState fields",
      .help_text = R"(Usage: news article show

Displays all fields of the current newspaper article state.
)",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }
        auto& ns = city->newspaper_state();
        std::vector<std::string> lines;
        lines.push_back("NewspaperState:");
        lines.push_back(compat::format("  article_id: {}", ns.article_id));
        lines.push_back(compat::format("  cooldown: {}", ns.cooldown));
        lines.push_back(compat::format("  polarity: {}", ns.polarity));
        lines.push_back(compat::format("  category: {}", ns.category));

        if (ns.target_ingredient) {
          lines.push_back(
              compat::format("  target_ingredient: {} ({})",
                             static_cast<uint8_t>(*ns.target_ingredient),
                             core::food::to_string(*ns.target_ingredient)));
        }
        else {
          lines.push_back("  target_ingredient: (none)");
        }

        if (ns.article_id == 0) {
          lines.push_back("  type: none");
        }
        else if (ns.is_ingredient_article()) {
          lines.push_back("  type: ingredient template (ARTIKEL3)");
        }
        else if (ns.is_event_article()) {
          auto event = static_cast<NewspaperEvent>(ns.event_article_index());
          lines.push_back(
              compat::format("  type: event article (ARTIKEL1, event={})",
                             core::newspaper::to_string(event)));
        }
        else {
          lines.push_back(compat::format(
              "  type: flavor text (ARTIKEL0, index={})", ns.article_id));
        }

        return CommandResult::success(lines);
      },
  };
}

static CommandDef make_news_article_set() {
  return CommandDef{
      .name = "set",
      .description = "Overwrite current article",
      .help_text = R"(Usage: news article set <article_id> [cooldown]

Sets article_id directly. Use 1-50 for flavor text, 51-100 for event articles,
or 200 (0xC8) for ingredient template. Cooldown defaults to 4.

Examples:
  news article set 55         # Event article for event 5
  news article set 200        # Ingredient template
  news article set 10 8       # Flavor text #10 with cooldown 8
)",
      .args =
          {
              ArgSpec::required("article_id", ArgType::Int, "Article ID"),
              ArgSpec{
                  .name = "cooldown",
                  .type = ArgType::Int,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "Cooldown days (default: 4)",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }
        auto article_id = ctx.int_arg(0);
        if (article_id < 0 || article_id > 255) {
          return CommandResult::error("Article ID must be 0-255");
        }

        auto& ns = city->newspaper_state();
        ns.article_id = static_cast<uint8_t>(article_id);
        ns.cooldown = ctx.args.size() > 1 ? static_cast<int>(ctx.int_arg(1))
                                          : NewspaperState::EVENT_COOLDOWN;

        return CommandResult::success().add_message(compat::format(
            "Set article_id={} cooldown={}", ns.article_id, ns.cooldown));
      },
  };
}

static CommandDef make_news_article_ingredient() {
  return CommandDef{
      .name = "ingredient",
      .description = "Set an ingredient article",
      .help_text =
          R"(Usage: news article ingredient <slot> <polarity> <category>

Sets article_id to 0xC8 (ingredient template) with the given parameters.

Arguments:
  slot      - Ingredient slot (0-95)
  polarity  - +1 or -1
  category  - Template category (0-3)

Examples:
  news article ingredient 0 1 0    # Tomatoes, positive, category 0
  news article ingredient 36 -1 2  # Salmon, negative, category 2
)",
      .args =
          {
              ArgSpec::required("slot", ArgType::Int, "Ingredient slot (0-95)"),
              ArgSpec::required("polarity", ArgType::Int, "+1 or -1"),
              ArgSpec::required("category", ArgType::Int, "Category (0-3)"),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto slot = ctx.int_arg(0);
        auto polarity = ctx.int_arg(1);
        auto category = ctx.int_arg(2);

        if (slot < 0 || slot >= max_ingredient_count) {
          return CommandResult::error(
              compat::format("Slot must be 0-{}", max_ingredient_count - 1));
        }
        if (polarity != 1 && polarity != -1) {
          return CommandResult::error("Polarity must be +1 or -1");
        }
        if (category < 0 || category > 3) {
          return CommandResult::error("Category must be 0-3");
        }

        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }
        auto& ns = city->newspaper_state();
        ns.article_id = NewspaperState::INGREDIENT_ARTICLE;
        ns.polarity = static_cast<int8_t>(polarity);
        ns.category = static_cast<uint8_t>(category);
        ns.target_ingredient = static_cast<IngredientType>(slot);
        ns.cooldown = NewspaperState::EVENT_COOLDOWN;

        auto name =
            core::food::is_valid_ingredient_id(static_cast<uint8_t>(slot))
                ? core::food::to_string(static_cast<IngredientType>(slot))
                : std::string_view("(invalid)");

        return CommandResult::success().add_message(compat::format(
            "Set ingredient article: {} (slot {}), polarity={}, category={}",
            name, slot, polarity, category));
      },
  };
}

static CommandDef make_news_article_clear() {
  return CommandDef{
      .name = "clear",
      .description = "Reset article to none",
      .help_text =
          "Usage: news article clear\n\nClears all NewspaperState fields.",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }
        city->newspaper_state().clear();
        city->newspaper_state().cooldown = 0;
        return CommandResult::success().add_message("Article state cleared");
      },
  };
}

static CommandDef make_news_article_read() {
  return CommandDef{
      .name = "read",
      .description = "Mark article as read (cooldown=0)",
      .help_text =
          "Usage: news article read\n\nSets cooldown to 0 so the article "
          "can be cleared on next daily update.",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }
        city->newspaper_state().mark_as_read();
        return CommandResult::success().add_message(
            "Article marked as read (cooldown=0)");
      },
  };
}

static CommandDef make_news_article() {
  return CommandDef{
      .name = "article",
      .description = "Manage newspaper article display state",
      .help_text = "Subcommands: show, set, ingredient, clear, read",
  }
      .add_subcommand(make_news_article_show())
      .add_subcommand(make_news_article_set())
      .add_subcommand(make_news_article_ingredient())
      .add_subcommand(make_news_article_clear())
      .add_subcommand(make_news_article_read());
}

// Trends subcommands

static TrendDuration parse_duration(const std::string& s) {
  if (s == "none") {
    return TrendDuration::None;
  }
  if (s == "linear") {
    return TrendDuration::Linear;
  }
  if (s == "exponential") {
    return TrendDuration::Exponential;
  }
  if (s == "countdown") {
    return TrendDuration::Countdown;
  }
  return TrendDuration::Count; // sentinel for "invalid"
}

static CommandDef make_news_trends_list() {
  return CommandDef{
      .name = "list",
      .description = "Show active ingredient trends",
      .help_text = R"(Usage: news trends list [all]

Shows trend slots with non-zero values, or all 96 if "all" is given.

Examples:
  news trends list            # Only active trends
  news trends list all        # All 96 slots
)",
      .args =
          {
              ArgSpec{
                  .name = "all",
                  .type = ArgType::String,
                  .optional = true,
                  .default_provider = DefaultProvider::EmptyString,
                  .description = "Pass 'all' to show all 96 slots",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }

        auto& trends = city->ingredient_trend_system();
        const auto& current = trends.trends();
        const auto& targets = trends.targets();
        const auto& durations = trends.durations();
        const auto& approaching = trends.approaching_flags();
        bool show_all = !ctx.args.empty() && ctx.string_arg(0) == "all";

        std::vector<std::string> lines;
        lines.push_back(
            compat::format("Ingredient trends for {}:", city->name()));

        int shown = 0;
        for (int i = 0; i < max_ingredient_count; ++i) {
          if (!show_all && current[i] == 0 &&
              durations[i] == TrendDuration::None) {
            continue;
          }

          auto name =
              core::food::is_valid_ingredient_id(static_cast<uint8_t>(i))
                  ? core::food::to_string(static_cast<IngredientType>(i))
                  : std::string_view("---");

          lines.push_back(compat::format(
              "  [{:2}] {:16} cur={:4} tgt={:4} dur={:12} {}", i, name,
              current[i], targets[i], to_string(durations[i]),
              approaching[i] ? "approaching" : "decaying"));
          ++shown;
        }

        if (shown == 0) {
          lines.push_back("  (no active trends)");
        }

        return CommandResult::success(lines);
      },
  };
}

static CommandDef make_news_trends_get() {
  return CommandDef{
      .name = "get",
      .description = "Full detail for one trend slot",
      .help_text = R"(Usage: news trends get <slot>

Shows all fields for a single trend slot.

Examples:
  news trends get 0           # Show trend for ingredient slot 0
  news trends get 36          # Show trend for slot 36 (Salmon)
)",
      .args =
          {
              ArgSpec::required("slot", ArgType::Int, "Ingredient slot (0-95)"),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto slot = ctx.int_arg(0);
        if (slot < 0 || slot >= max_ingredient_count) {
          return CommandResult::error(
              compat::format("Slot must be 0-{}", max_ingredient_count - 1));
        }

        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }

        auto& trends = city->ingredient_trend_system();
        auto i = static_cast<uint8_t>(slot);

        auto name = core::food::is_valid_ingredient_id(i)
                        ? core::food::to_string(static_cast<IngredientType>(i))
                        : std::string_view("(invalid)");

        std::vector<std::string> lines;
        lines.push_back(compat::format("Trend slot {} ({}):", slot, name));
        lines.push_back(compat::format("  current: {}", trends.trends()[i]));
        lines.push_back(compat::format("  target: {}", trends.targets()[i]));
        lines.push_back(
            compat::format("  duration: {}", to_string(trends.durations()[i])));
        lines.push_back(compat::format(
            "  phase: {}",
            trends.approaching_flags()[i] ? "approaching" : "decaying"));

        return CommandResult::success(lines);
      },
  };
}

static CommandDef make_news_trends_set() {
  return CommandDef{
      .name = "set",
      .description = "Apply a TrendEffect to a slot",
      .help_text =
          R"(Usage: news trends set <slot> <initial> <target> <duration>

Applies a trend effect with the given parameters.
Duration: "none", "linear", "exponential", "countdown".

Examples:
  news trends set 0 10 50 linear         # Tomatoes: +10 initial, +50 target, linear
  news trends set 36 -20 0 exponential   # Salmon: -20 initial, 0 target, exp decay
)",
      .args =
          {
              ArgSpec::required("slot", ArgType::Int, "Ingredient slot (0-95)"),
              ArgSpec::required("initial", ArgType::Int,
                                "Initial value (-128 to 127)"),
              ArgSpec::required("target", ArgType::Int,
                                "Target value (-128 to 127)"),
              ArgSpec::required("duration", ArgType::String,
                                "none/linear/exponential/countdown"),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto slot = ctx.int_arg(0);
        auto initial = ctx.int_arg(1);
        auto target = ctx.int_arg(2);
        auto dur_str = ctx.string_arg(3);

        if (slot < 0 || slot >= max_ingredient_count) {
          return CommandResult::error(
              compat::format("Slot must be 0-{}", max_ingredient_count - 1));
        }
        if (initial < -128 || initial > 127) {
          return CommandResult::error("Initial must be -128 to 127");
        }
        if (target < -128 || target > 127) {
          return CommandResult::error("Target must be -128 to 127");
        }

        auto dur = parse_duration(dur_str);
        if (dur == TrendDuration::Count) {
          return CommandResult::error(
              "Duration must be: none, linear, exponential, countdown");
        }

        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }

        city->ingredient_trend_system().apply_effect(
            static_cast<IngredientType>(slot),
            TrendEffect{
                .initial_value = static_cast<int>(initial),
                .control = static_cast<int>(target),
                .duration = dur,
            });

        // Also set the newspaper article so the player sees it
        auto& ns = city->newspaper_state();
        ns.article_id = NewspaperState::INGREDIENT_ARTICLE;
        ns.target_ingredient = static_cast<IngredientType>(slot);
        ns.cooldown = NewspaperState::EVENT_COOLDOWN;

        // Derive polarity from whichever value is nonzero
        int sign_source = static_cast<int>(initial != 0 ? initial : target);
        ns.polarity = static_cast<int8_t>(sign_source >= 0 ? 1 : -1);

        // Derive category from duration type
        switch (dur) {
        case TrendDuration::Linear:
          ns.category = 1;
          break;
        case TrendDuration::Countdown:
          ns.category = 2;
          break;
        default: // Exponential
          ns.category = 0;
          break;
        }

        auto name =
            core::food::is_valid_ingredient_id(static_cast<uint8_t>(slot))
                ? core::food::to_string(static_cast<IngredientType>(slot))
                : std::string_view("(invalid)");

        return CommandResult::success().add_message(compat::format(
            "Applied trend to slot {} ({}): initial={} target={} "
            "duration={} (article set, polarity={}, cat={})",
            slot, name, initial, target, dur_str, ns.polarity, ns.category));
      },
  };
}

static CommandDef make_news_trends_clear() {
  return CommandDef{
      .name = "clear",
      .description = "Clear one or all trend slots",
      .help_text = R"(Usage: news trends clear [slot]

Clears a specific slot (0-95) or all slots if no argument given.

Examples:
  news trends clear           # Clear all trends
  news trends clear 0         # Clear slot 0 only
)",
      .args =
          {
              ArgSpec{
                  .name = "slot",
                  .type = ArgType::Int,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "Slot index (0-95), or omit for all",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }

        auto& trends = city->ingredient_trend_system();

        if (!ctx.args.empty()) {
          auto slot = ctx.int_arg(0);
          if (slot < 0 || slot >= max_ingredient_count) {
            return CommandResult::error(
                compat::format("Slot must be 0-{}", max_ingredient_count - 1));
          }
          trends.reset_slot(static_cast<IngredientType>(slot));
          return CommandResult::success().add_message(
              compat::format("Cleared trend slot {}", slot));
        }

        trends.clear();
        return CommandResult::success().add_message(
            "Cleared all ingredient trend slots");
      },
  };
}

static CommandDef make_news_trends_tick() {
  return CommandDef{
      .name = "tick",
      .description = "Advance trend system by n days",
      .help_text = R"(Usage: news trends tick [n]

Calls daily_tick() n times (default 1) to advance trends.

Examples:
  news trends tick            # Advance 1 day
  news trends tick 5          # Advance 5 days
)",
      .args =
          {
              ArgSpec{
                  .name = "n",
                  .type = ArgType::Int,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "Number of days to advance (default: 1)",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }

        int64_t n = !ctx.args.empty() ? ctx.int_arg(0) : 1;
        if (n < 1 || n > 1000) {
          return CommandResult::error("N must be 1-1000");
        }

        for (int64_t i = 0; i < n; ++i) {
          city->ingredient_trend_system().daily_tick();
        }

        return CommandResult::success().add_message(
            compat::format("Advanced trend system by {} day(s)", n));
      },
  };
}

static CommandDef make_news_trends() {
  return CommandDef{
      .name = "trends",
      .description = "Manage ingredient trend system",
      .help_text = "Subcommands: list, get, set, clear, tick",
  }
      .add_subcommand(make_news_trends_list())
      .add_subcommand(make_news_trends_get())
      .add_subcommand(make_news_trends_set())
      .add_subcommand(make_news_trends_clear())
      .add_subcommand(make_news_trends_tick());
}

// Root news command

static CommandDef make_news_status() {
  return CommandDef{
      .name = "status",
      .description = "Show newspaper status overview",
      .help_text =
          "Usage: news status\n\nShows article, schedule, and trend summary.",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }
        auto& ns = city->newspaper_state();
        auto& sys = city->newspaper_event_system();

        std::vector<std::string> lines;
        lines.push_back("Newspaper status:");

        // Article summary
        if (ns.article_id == 0) {
          lines.push_back("  Article: none");
        }
        else if (ns.is_ingredient_article()) {
          auto name =
              ns.target_ingredient
                  ? std::string(core::food::to_string(*ns.target_ingredient))
                  : std::string("(none)");
          lines.push_back(compat::format(
              "  Article: ingredient template ({}), polarity={}, cat={}", name,
              ns.polarity, ns.category));
        }
        else if (ns.is_event_article()) {
          auto event = static_cast<NewspaperEvent>(ns.event_article_index());
          lines.push_back(compat::format("  Article: event {} ({})",
                                         ns.event_article_index(),
                                         core::newspaper::to_string(event)));
        }
        else {
          lines.push_back(
              compat::format("  Article: flavor text #{}", ns.article_id));
        }
        lines.push_back(compat::format("  Cooldown: {}", ns.cooldown));

        // Scheduled events count
        int scheduled = 0;
        const auto& types = sys.scheduled_types();
        const auto& countdowns = sys.scheduled_countdowns();
        for (int i = 0; i < 50; ++i) {
          if (static_cast<int>(types[i]) != 0 || countdowns[i] != 0) {
            ++scheduled;
          }
        }
        lines.push_back(compat::format("  Scheduled events: {}/50", scheduled));

        // Active trends count
        auto& trends = city->ingredient_trend_system();
        int active = 0;
        for (int i = 0; i < max_ingredient_count; ++i) {
          if (trends.durations()[i] != TrendDuration::None) {
            ++active;
          }
        }
        lines.push_back(compat::format("  Active trends: {}/{} ({})", active,
                                       max_ingredient_count, city->name()));

        return CommandResult::success(lines);
      },
  };
}

CommandDef make_newspaper_commands() {
  return CommandDef{
      .name = "news",
      .description = "Newspaper and trend system",
      .help_text = "Subcommands: status, schedule, article, trends",
  }
      .add_subcommand(make_news_status())
      .add_subcommand(make_news_schedule())
      .add_subcommand(make_news_article())
      .add_subcommand(make_news_trends());
}

} // namespace debug::commands
