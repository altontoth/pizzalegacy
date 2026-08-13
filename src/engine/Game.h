/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "engine/SDLSession.h"
#include "engine/SDL_include.h"

#include "audio/AudioManager.h"
#include "core/util/ConfigManager.h"
#include "core/util/GameContext.h"
#include "engine/DisplayConstants.h"
#include "engine/SDLContext.h"
#include "engine/input/GlobalActionHandler.h"
#include "engine/ui/CursorManager.h"
#include "engine/ui/InteractionManager.h"
#include "resources/BlitmapManager.h"
#include "resources/BobManager.h"
#include "resources/FontManager.h"
#include "resources/VgaManager.h"
#include "views/base/View.h"
#include "views/helpers//OverlayManager.h"

class ConsoleLayer;
class DebugConsole;

class Game {
public:
  struct LaunchOptions {
    std::string config_path;              // empty = use platform default
    std::optional<std::string> game_path; // --game-path
    std::optional<std::string>
        viewer_target; // positional <file|fonts|pal|char>
    // "console <...>", one entry per argv word
    std::optional<std::vector<std::string>> console_args;
    std::optional<std::string> exec_script; // --exec <file>, then exit
    std::optional<uint32_t> rng_seed;       // --seed <n>
    std::optional<bool> crt_mode;           // --crt to force CRT mode
    std::optional<std::string> vga_viewer;  // --vga <path>
    std::optional<std::string> bob_viewer;  // --bob <path>
  };

  Game(LaunchOptions opts);
  ~Game();
  // Process exit code; non-zero when a --exec script had a command fail.
  int run();

private:
  struct LoopState {
    bool fullscreen = false;
    OverlayManager* overlays = nullptr;
    GameState& game_state;
    GameContext& context;
    bool screenshot_requested = false;
    bool fade_on_next_transition = false;
    bool skip_tick_during_fade_in = false;
  };

  bool init(int display_height, ConfigManager& config_manager);
  Renderable* change_state(GameState& gs);
  bool enforce_aspect_ratio(SDL_Window*, int, int);
  void create_some_players(GameState&);
  void load_assets();
  void handle_events(LoopState& loop_state);
  void handle_resize_event(const SDL_Event& event, LoopState& loop_state);
  ScreenPoint resolve_motion_position(const SDL_MouseMotionEvent& motion) const;
  ScreenPoint global_cursor_pos_clamped() const;
  static int get_display_height(bool crt_mode);
  SDL_Texture* create_display_texture(int display_height) const;

  // SDLSession needs to be declared first so it's destroyed last:
  SDLSession sdl_session_;

  bool window_resizing_in_progress_{false};
  int display_height_{DisplayConstants::SCREEN_HEIGHT};
  std::optional<bool> crt_mode_override_;
  std::unique_ptr<BobManager> bob_manager_;
  std::unique_ptr<FontManager> font_manager_;
  std::unique_ptr<VgaManager> vga_manager_;
  std::unique_ptr<BlitmapManager> blitmap_manager_;
  std::unique_ptr<CursorManager> cursor_manager_;
  std::unique_ptr<TooltipManager> tooltip_manager_;
  std::unique_ptr<views::ui::InteractionManager> interaction_manager_;
  std::unique_ptr<input::InputManager> input_manager_{nullptr};
  std::unique_ptr<audio::AudioManager> audio_manager_;
  std::unique_ptr<View> current_view_{nullptr};
  std::unique_ptr<GlobalActionHandler> global_actions_;
  GameContext context;
  SDLContext sdl_;
  SDL_Surface_unique_ptr screen_;
  Surface surface_;
  ScreenPoint last_mouse_pos_;
  bool prev_relative_mouse_mode_ = false;
  ScreenPoint pre_relative_mouse_pos_;
  std::string filename_;
  std::string config_path_;
  std::optional<std::string> game_path_override_;
  std::optional<std::vector<std::string>> startup_console_args_;
  std::optional<std::string> exec_script_;
  std::optional<uint32_t> rng_seed_;

  // Returns process exit code.
  int run_exec_script(DebugConsole& console, GameState& state) const;
  std::optional<std::string> vga_viewer_path_;
  std::optional<std::string> bob_viewer_path_;
};
