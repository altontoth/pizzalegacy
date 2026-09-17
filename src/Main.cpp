/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "engine/Game.h"
#include "engine/SDL_include.h"
#include "engine/Version.h"

namespace { // anonymous
[[noreturn]] void version() {
  std::cout << "Version: Pizza Legacy " << game_meta::VERSION << "\n";
  std::cout << "Copyright: 2026 Cowomaly\n";
  std::cout << "License: GPL-3.0-or-later\n";
  std::cout << "Website: https://pizzalegacy.nl/\n";
  std::cout << "Repository: https://codeberg.org/cowomaly/pizzalegacy/\n";
  std::exit(0);
}

[[noreturn]] void usage(const char* prog, int code) {
  std::cerr
      << "Usage:\n"
      << "  " << prog << "                        # normal boot (Intro)\n"
      << "  " << prog << " <file|fonts|sounds|pal|char>  # open viewer\n"
      << "  " << prog
      << " console <cmd...>       # run one console line on startup\n"
      << "Options:\n"
      << "  --config <file>                        # config path (default: "
         "platform pref dir)\n"
      << "  --game-path <dir>                      # override game data dir\n"
      << "  --exec <file>                          # run console commands "
         "from a file, then exit\n"
      << "  --seed <n>                             # specify the RNG seed\n"
      << "  --vga <path>                           # open VGA viewer with "
         "file\n"
      << "  --bob <path>                           # open BOB viewer with "
         "file\n"
      << "  --card <text>                          # show a title card, '|' "
         "breaks a line\n"
      << "  --crt                                  # force CRT aspect ratio "
         "(4:3)\n"
      << "  -h, --help\n";
  std::exit(code);
}
} // namespace

static Game::LaunchOptions parse_args(int argc, char* argv[]) {
  Game::LaunchOptions opts;
  bool seen_positional = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      usage(argv[0], 0);
    }
    if (arg == "-v" || arg == "--version") {
      version();
    }
    else if (arg == "--") {
      // Everything after '--' is the first positional (viewer target).
      if (i + 1 < argc) {
        opts.viewer_target = std::string(argv[i + 1]);
      }
      break;
    }
    else if (arg == "--config") {
      if (i + 1 >= argc) {
        usage(argv[0], 2);
      }
      opts.config_path = argv[++i];
    }
    else if (arg == "--game-path") {
      if (i + 1 >= argc) {
        usage(argv[0], 2);
      }
      opts.game_path = std::string(argv[++i]);
    }
    else if (arg == "--exec") {
      if (i + 1 >= argc) {
        usage(argv[0], 2);
      }
      opts.exec_script = std::string(argv[++i]);
    }
    else if (arg == "--seed") {
      if (i + 1 >= argc) {
        usage(argv[0], 2);
      }
      opts.rng_seed =
          static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
    }
    else if (arg == "--crt") {
      opts.crt_mode = true;
    }
    else if (arg == "--vga") {
      if (i + 1 >= argc) {
        usage(argv[0], 2);
      }
      opts.vga_viewer = argv[++i];
    }
    else if (arg == "--bob") {
      if (i + 1 >= argc) {
        usage(argv[0], 2);
      }
      opts.bob_viewer = argv[++i];
    }
    else if (arg == "--card") {
      if (i + 1 >= argc) {
        usage(argv[0], 2);
      }
      opts.card_text = argv[++i];
    }
    else if (arg == "console") {
      // The rest is the console command, one argument per word as the shell
      // split it.
      std::vector<std::string> args;
      for (int j = i + 1; j < argc; ++j) {
        args.emplace_back(argv[j]);
      }
      if (args.empty()) {
        std::cerr << "error: 'console' requires a command.\n";
        usage(argv[0], 2);
      }
      opts.console_args = std::move(args);
      // 'console' subcommand consumes the rest; ignore any other positionals.
      break;
    }
    else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "error: unknown option: " << arg << "\n";
      usage(argv[0], 2);
    }
    else {
      // Positional argument -> viewer target (keep only the first one).
      if (!seen_positional) {
        opts.viewer_target = arg;
        seen_positional = true;
      }
      else {
        std::cerr << "warning: extra positional ignored: " << arg << "\n";
      }
    }
  }

  return opts;
}

int main(int argc, char* argv[]) {

  Game::LaunchOptions opts = parse_args(argc, argv);

  auto game = std::make_unique<Game>(std::move(opts));
  return game->run();
}
