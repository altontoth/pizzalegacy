/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/debug/ScreenCommands.h"

#include <string>
#include <vector>

#include "core/debug/command/CommandDef.h"
#include "core/util/GameContext.h"
#include "engine/Game.h"
#include "engine/PlatformPaths.h"
#include "engine/Renderer.h"
#include "engine/SDL_include.h"
#include "engine/Screenshot.h"
#include "engine/gfx/Surface.h"
#include "resources/VgaManager.h"

namespace engine {

std::vector<debug::command::CommandDef>
make_screen_commands(Game& game, GameContext& context, GameState& state) {
  std::vector<debug::command::CommandDef> commands;

  commands.push_back(debug::command::CommandDef{
      .name = "screenshot",
      .description = "Write the current screen to a PNG",
      .help_text = R"(Renders whatever the UI stack has on top and saves it.

A bare filename goes to the screenshots directory; give a path with a
directory in it to put it somewhere else. Without an argument the name is
the timestamp.

Examples:
  screenshot
  screenshot stats.png
  screenshot /tmp/stats.png
)",
      .args =
          {
              debug::command::ArgSpec{
                  .name = "file",
                  .type = debug::command::ArgType::String,
                  .optional = true,
                  .description = "Where to write the PNG",
              },
          },
      .handler = [&game, &state](const debug::command::CommandContext& ctx)
          -> debug::command::CommandResult {
        const std::string name = ctx.has_arg(0) ? ctx.string_arg(0) : "";
        const std::string path = resolve_screenshot_path(name);
        if (!game.capture_screenshot(state, path)) {
          return debug::command::CommandResult::error(
              "Nothing was written to " + path);
        }
        return debug::command::CommandResult::success({"Wrote " + path});
      },
  });

  commands.push_back(debug::command::CommandDef{
      .name = "dumpgameloc",
      .description = "Write the game location screen picture to a PNG",
      .help_text = R"(Draws the picture shown before the game folder is known.

Example:
  dumpgameloc /tmp/gameloc.png
)",
      .args =
          {
              debug::command::ArgSpec{
                  .name = "file",
                  .type = debug::command::ArgType::String,
                  .description = "Where to write the PNG",
              },
          },
      .handler = [&context](const debug::command::CommandContext& ctx)
          -> debug::command::CommandResult {
        const std::string path = ctx.string_arg(0);
        const Surface& picture =
            context.vga->get(platform_paths::data_dir() + "game_location");
        SDL_Surface_unique_ptr frame = Renderer::to_sdl_surface(picture, 1.0f);
        if (!frame || !save_screenshot(frame.get(), path)) {
          return debug::command::CommandResult::error(
              "Nothing was written to " + path);
        }
        return debug::command::CommandResult::success({"Wrote " + path});
      },
  });

  commands.push_back(debug::command::CommandDef{
      .name = "savevga",
      .description = "Write the current screen to a VGA file",
      .help_text = R"(Renders whatever the UI stack has on top and encodes it
as a VGA file, the format the original game's pictures use.

Screens are drawn at twice the original's size, so the file stores every
second pixel.

Example:
  savevga data/game_location.vga
)",
      .args =
          {
              debug::command::ArgSpec{
                  .name = "file",
                  .type = debug::command::ArgType::String,
                  .description = "Where to write the VGA",
              },
          },
      .handler = [&game, &state](const debug::command::CommandContext& ctx)
          -> debug::command::CommandResult {
        const std::string path = ctx.string_arg(0);
        if (!game.write_screen_as_vga(state, path)) {
          return debug::command::CommandResult::error(
              "Nothing was written to " + path);
        }
        return debug::command::CommandResult::success({"Wrote " + path});
      },
  });

  return commands;
}

} // namespace engine
