/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/Game.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <numeric>

#include <magic_enum/magic_enum.hpp>
#include "conversations/BankConversation.h"
#include "conversations/MafiaConversation.h"
#include "conversations/MafiaLine.h"
#include "conversations/MayorConversation.h"
#include "conversations/PoliceConversation.h"
#include "conversations/SupplierConversation.h"
#include "core/character/Player.h"
#include "core/debug/DebugCommands.h"
#include "core/food/IngredientCategory.h"
#include "core/save/SaveGameManager.h"
#include "core/state/Overloaded.h"
#include "core/state/UIState.h"
#include "core/util/ConfigManager.h"
#include "engine/DebugConsole.h"
#include "engine/FpsManager.h"
#include "engine/Logger.h"
#include "engine/PlatformPaths.h"
#include "engine/Random.h"
#include "engine/Renderer.h"
#include "engine/SDL_include.h"
#include "engine/Version.h"
#include "engine/input/InputManager.h"
#include "engine/ui/CursorManager.h"
#include "engine/ui/TooltipManager.h"
#include "factories/CharacterFactory.h"
#include "gfx/Surface.h"
#include "resources/VgaLoader.h"
#include "views/base/PaneledView.h"
#include "views/base/View.h"
#include "views/factories/ActorViewFactory.h"
#include "views/layers/ConsoleLayer.h"
#include "views/layers/DebugLayer.h"
#include "views/layers/PauseLayer.h"
#include "views/layers/TooltipLayer.h"
#include "views/screens/ActorView.h"
#include "views/screens/Advertising.h"
#include "views/screens/AnimatedStats.h"
#include "views/screens/CookbookReminder.h"
#include "views/screens/EuroMap.h"
#include "views/screens/FullscreenStatus.h"
#include "views/screens/GameOver.h"
#include "views/screens/GlobalStats.h"
#include "views/screens/Intro.h"
#include "views/screens/MafiaMissionView.h"
#include "views/screens/MarketResearch.h"
#include "views/screens/Newspaper.h"
#include "views/screens/PizzaDesign.h"
#include "views/screens/PizzaJudges.h"
#include "views/screens/PlayerProfile.h"
#include "views/screens/PlayerTurnCard.h"
#include "views/screens/StaffView.h"
#include "views/screens/StatsRestaurants.h"
#include "views/screens/Terminal.h"
#include "views/screens/WinSequenceView.h"
#include "views/screens/city_view/CityView.h"
#include "views/screens/depot/DepotExportScreen.h"
#include "views/screens/depot/DepotMain.h"
#include "views/screens/depot/DepotWeaponInterface.h"
#include "views/screens/menus/CharacterSelection.h"
#include "views/screens/menus/DifficultySelection.h"
#include "views/screens/menus/DiskMenu.h"
#include "views/screens/menus/MenuGameLoad.h"
#include "views/screens/menus/MenuHighScores.h"
#include "views/screens/menus/MenuMain.h"
#include "views/screens/menus/MenuNewGame.h"
#include "views/screens/menus/MenuOptions.h"
#include "views/screens/menus/MenuQuickstart.h"
#include "views/screens/menus/MenuSave.h"
#include "views/screens/menus/MenuSaveAsGame.h"
#include "views/screens/purchasing/CategoryOverviewScreen.h"
#include "views/screens/purchasing/Demographics.h"
#include "views/screens/purchasing/FloorListView.h"
#include "views/screens/purchasing/FurnitureListView.h"
#include "views/screens/purchasing/PurchasingMain.h"
#include "views/screens/purchasing/SupplierList.h"
#include "views/screens/restaurant/RestaurantMenu.h"
#include "views/screens/restaurant/RestaurantStats.h"
#include "views/screens/restaurant/RestaurantView.h"
#include "views/screens/restaurant/WeeksTakings.h"
#include "views/screens/stock/Contracts.h"
#include "views/screens/stock/DepotIngredientCategoryView.h"
#include "views/screens/stock/DepotIngredientOrderScreen.h"
#include "views/screens/stock/DepotIngredientSupplierList.h"
#include "views/screens/stock/IngredientCategoryView.h"
#include "views/screens/stock/IngredientOrderScreen.h"
#include "views/screens/stock/IngredientSupplierList.h"
#include "views/screens/stock/StockMain.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/ConfirmPopup.h"
#include "views/viewers/BobViewer.h"
#include "views/viewers/CharacterViewer.h"
#include "views/viewers/FontViewer.h"
#include "views/viewers/PaletteView.h"
#include "views/viewers/SoundViewer.h"
#include "views/viewers/VgaViewer.h"

namespace {
SDL_Surface* format_for_png_fixed(SDL_Surface* input) {
  // Create a surface with explicitly defined RGBA masks;
  // when using SDL_PNGFormatAlpha something was wrong with the colors
  SDL_Surface* output =
      SDL_CreateSurface(input->w, input->h, SDL_PIXELFORMAT_ABGR8888);

  if (!output) {
    Logger::warn("Failed to create RGB surface: {}", SDL_GetError());
    return nullptr;
  }

  if (!SDL_BlitSurface(input, nullptr, output, nullptr)) {
    Logger::warn("Failed to blit surface: {}", SDL_GetError());
    SDL_DestroySurface(output);
    return nullptr;
  }

  return output;
}

std::string generate_screenshot_filename() {
  std::time_t t = std::time(nullptr);
  std::tm tm;
#if defined(_WIN32)
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif

  std::ostringstream oss;
  oss << "screenshot_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".png";
  return oss.str();
}

void save_screenshot(SDL_Surface* surface) {
  namespace fs = std::filesystem;
  std::string dir = platform_paths::screenshots_dir();
  if (!fs::exists(dir)) {
    fs::create_directories(dir);
  }
  std::string full_filename = dir + generate_screenshot_filename();

  SDL_Surface* tmp = format_for_png_fixed(surface);
  if (!tmp) {
    Logger::warn("Failed to format surface for PNG");
    return;
  }
  SDL_Surface_unique_ptr png_surface(tmp);
  if (!SDL_SavePNG(png_surface.get(), full_filename.c_str())) {
    Logger::warn("Failed to save PNG: {}", SDL_GetError());
  }
  else {
    Logger::info("Screenshot saved as {}", full_filename);
  }
}

} // end namespace

Game::Game(LaunchOptions opts)
    : crt_mode_override_(opts.crt_mode),
      surface_(gfx::Surface(DisplayConstants::SCREEN_WIDTH,
                            DisplayConstants::SCREEN_HEIGHT)),
      filename_(opts.viewer_target.value_or("")),
      config_path_(std::move(opts.config_path)),
      game_path_override_(std::move(opts.game_path)),
      startup_console_line_(std::move(opts.console_line)),
      vga_viewer_path_(std::move(opts.vga_viewer)),
      bob_viewer_path_(std::move(opts.bob_viewer)) {}

// Define this here instead of the header to let forward declares work inside
// unique_ptrs
Game::~Game() = default;

void Game::run() {

  if (!sdl_session_.initialized()) {
    std::cerr << "SDL failed to init: " << SDL_GetError() << "\n";
    return;
  }
  platform_paths::init();

  // Configure platform-aware saves directory
  save::SaveGameManager::set_saves_directory(platform_paths::saves_dir());

  // Use CLI-provided config path, or fall back to platform default
  std::string effective_config =
      config_path_.empty() ? platform_paths::config_file_path() : config_path_;
  ConfigManager configManager(effective_config);

  // Load config if present
  const bool had_config = configManager.load_config();

  // If --game-path was provided on CLI, take it (and persist).
  if (game_path_override_) {
    configManager.value("game_path_english", *game_path_override_);
    configManager.save_config();
  }
  // Otherwise, if no config file, prompt
  else if (!had_config) {
    std::string location = configManager.prompt_for_game_location();
    if (location.empty()) {
      std::cout << "Didn't find the original game, giving up.\n";
      return;
    }
    configManager.value("game_path_english", location);
    configManager.save_config();
  }

  // Use format-specific path based on active_asset_format
  std::string active_path = configManager.active_game_path();
  if (active_path.empty()) {
    // No valid path found - prompt user to select game location
    active_path = configManager.prompt_for_game_location();
    if (active_path.empty()) {
      std::cout << "No valid game path found, exiting.\n";
      return;
    }
    configManager.save_config();
  }
  fileutils::game_data_path(active_path);

  // Load options early to determine display height
  auto loaded_options = configManager.load_options();

  // Override with command line --crt flag if provided
  if (crt_mode_override_) {
    loaded_options.crt_aspect_ratio = *crt_mode_override_;
  }

  const int display_height =
      get_display_height(loaded_options.crt_aspect_ratio);

  if (!init(display_height, configManager)) {
    std::cerr << "Init failed!\n";
    return;
  }

  // Set up ConfigManager in context for options access
  context.config = &configManager;

  GameState state;

  // Use the already-loaded options
  state.options() = loaded_options;

  // For direct file loading (--vga, --bob, --fnt, --raw), set up the state with
  // the correct viewer type
  if (vga_viewer_path_) {
    ViewerState vs;
    vs.filename = *vga_viewer_path_;
    vs.type = ViewerType::Vga;
    state.ui().push<ViewerState>(vs);
  }
  else if (bob_viewer_path_) {
    ViewerState vs;
    vs.filename = *bob_viewer_path_;
    vs.type = ViewerType::Bob;
    state.ui().push<ViewerState>(vs);
  }
  else if (!filename_.empty()) {
    state.ui().push<ViewerState>(filename_);
  }
  else {
    state.ui().push<ScreenTag>(ScreenTag::Kind::Intro);
  }

  {
    int ww;
    int wh;
    SDL_GetWindowSize(sdl_.window.get(), &ww, &wh);
    state.window_size(ww, wh);
    context.cursor->update_scaled_cursor(ww, wh);
  }
  create_some_players(state);

  // Main game loop prep
  DebugConsole debug_console;
  register_debug_commands(debug_console, state, context);

  // And logger that logs to in-game console
  Logger::add_sink(debug_console.make_sink());
  Logger::info("Logger initialized");

  OverlayManager overlays;
  overlays.push<TooltipLayer>(context, state, *tooltip_manager_);

  auto& popup_mgr = overlays.push<PopupManager>(context);
  context.popup = &popup_mgr;
  overlays.push<PauseLayer>(context, state);

  auto& debug_mgr = overlays.push<DebugLayer>(context, state);
  interaction_manager_->on_hot_changed(
      [&debug_mgr] { debug_mgr.mark_dirty(); });

  overlays.push<ConsoleLayer>(context, state, debug_console);

  LoopState loop_state = {
      .fullscreen = false,
      .overlays = &overlays,
      .game_state = state,
      .context = context,
      .screenshot_requested = false,
  };

  // after input_manager_ is constructed and DefaultBindings applied
  GlobalActionHandler::Callbacks cb{
      .request_exit =
          [&loop_state, &popup_mgr] {
            popup_mgr.origin({150, 130});
            popup_mgr.create<ConfirmPopup>(
                "Are you sure you want to exit?", [loop_state](bool agree) {
                  if (agree) {
                    loop_state.game_state.request_exit();
                  }
                });
          },
      .request_screenshot =
          [&loop_state] { loop_state.screenshot_requested = true; },
      .toggle_fullscreen =
          [this, &loop_state] {
            loop_state.fullscreen = !loop_state.fullscreen;
            SDL_SetWindowFullscreen(sdl_.window.get(), loop_state.fullscreen);
          }};
  global_actions_ =
      std::make_unique<GlobalActionHandler>(*input_manager_, std::move(cb));

  // If provided, run a console line immediately (no need to open the UI
  // console, save some time during development)
  if (startup_console_line_) {
    debug_console.execute_command(*startup_console_line_);
  }

  Renderer renderer(sdl_, display_height_);
  renderer.set_cursor_manager(cursor_manager_.get());
  cursor_manager_->software_mode(state.options().software_cursor);
  SDL_SetRenderVSync(sdl_.renderer.get(),
                     state.options().vsync ? 1 : SDL_RENDERER_VSYNC_DISABLED);
  renderer.add_layers(change_state(state, loop_state));
  std::ranges::for_each(overlays.renderables(),
                        [&](Renderable* r) { renderer.add_layers(r); });

  constexpr uint32_t TICK_MS = 1000 / FRAMES_PER_SECOND;
  constexpr int MAX_TICKS_PER_FRAME = 5;

  auto apply_window_size = [&](int target_w, int target_h) {
    SDL_SetWindowSize(sdl_.window.get(), target_w, target_h);
    int actual_w;
    int actual_h;
    SDL_GetWindowSize(sdl_.window.get(), &actual_w, &actual_h);
    state.window_size(actual_w, actual_h);
    loop_state.context.cursor->update_scaled_cursor(actual_w, actual_h);
  };

  int render_fps = static_cast<int>(state.options().render_fps);
  bool vsync = state.options().vsync;
  FpsManager fps(render_fps);
  uint32_t tick_accumulator_ms = 0;

  while (!state.should_exit()) {
    uint64_t wall_ms = SDL_GetTicks();
    context.clock.begin_frame(wall_ms);
    fps.begin_frame(wall_ms);

    // Apply changes in FPS from the options menu.
    if (static_cast<int>(state.options().render_fps) != render_fps) {
      render_fps = static_cast<int>(state.options().render_fps);
      fps.set_target_fps(render_fps);
    }
    if (state.options().vsync != vsync) {
      vsync = state.options().vsync;
      SDL_SetRenderVSync(sdl_.renderer.get(),
                         vsync ? 1 : SDL_RENDERER_VSYNC_DISABLED);
    }

    if (!current_view_) {
      Logger::warn("Current view is nullptr!");
      std::exit(1);
    }

    handle_events(loop_state);

    // Logic runs independent of FPS
    tick_accumulator_ms += context.clock.dt_ms;
    int ticks_this_frame = 0;
    while (tick_accumulator_ms >= TICK_MS &&
           ticks_this_frame < MAX_TICKS_PER_FRAME) {
      tick_accumulator_ms -= TICK_MS;
      ++ticks_this_frame;

      input_manager_->tick();

      // Skip ticking view during fade-in to avoid missing animations/info
      bool should_skip_tick = renderer.is_fading();

      if (!overlays.game_blocked() && !should_skip_tick) {
        current_view_->tick();
      }
      overlays.tick_all();

      state.update(TICK_MS);

      // Handle fade transitions
      bool fade_completed = renderer.tick_fade();

      // Clear skip flag when fade completes
      if (fade_completed && loop_state.skip_tick_during_fade_in) {
        loop_state.skip_tick_during_fade_in = false;
      }

      // Check if resolution change was requested
      if (auto res = state.pending_resolution_change()) {
        state.clear_resolution_request();
        auto [w, h] = resolution_window_size(*res, display_height_);
        apply_window_size(w, h);
      }

      // Check if CRT aspect ratio change was requested
      if (auto crt = state.pending_crt_change()) {
        state.clear_crt_request();
        display_height_ = get_display_height(*crt);
        sdl_.texture.reset(create_display_texture(display_height_));
        SDL_SetRenderLogicalPresentation(
            sdl_.renderer.get(), DisplayConstants::SCREEN_WIDTH,
            display_height_, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        renderer.set_display_height(display_height_);
        interaction_manager_->set_display_height(display_height_);
        cursor_manager_->display_height(display_height_);
        auto [w, h] =
            resolution_window_size(state.options().resolution, display_height_);
        apply_window_size(w, h);
      }

      // Check if fade requested and not already fading
      if (state.fade_requested() && !renderer.is_fading()) {
        // Include pending_fade_midpoint: a UI push is about to happen at the
        // midpoint (while black), so we must expect a transition even if the
        // UI stack hasn't changed yet.
        loop_state.fade_on_next_transition =
            state.ui().ui_changed() || state.pending_fade_midpoint();
        state.clear_fade_request();
        renderer.start_fade_out(state.fade_duration_frames());
      }

      const bool fade_out_finished_and_pending =
          renderer.fade_out_complete() && loop_state.fade_on_next_transition;

      // Don't transition while a popup is active; the popup must be
      // dismissed first so it shows on the current view, not the new one.
      // game_blocked() before ui_changed() so short-circuit preserves the flag.
      const bool normal_ui_transition = !renderer.is_fading() &&
                                        !overlays.game_blocked() &&
                                        state.ui().ui_changed();

      if (fade_out_finished_and_pending || normal_ui_transition) {
        interaction_manager_->clear();
        tooltip_manager_->clear();

        if (fade_out_finished_and_pending) {
          // Fire midpoint callback while black so we can do the date advance
          // then
          state.on_fade_midpoint();
        }

        // Get new view and check if it wants to preserve background
        auto* new_view = change_state(state, loop_state);
        if (new_view && new_view->preserve_background()) {
          renderer.clear_layers_preserve_backbuffer();
        }
        else {
          renderer.clear_layers();
        }
        renderer.add_layers(new_view);
        std::ranges::for_each(overlays.renderables(),
                              [&](Renderable* r) { renderer.add_layers(r); });

        if (fade_out_finished_and_pending) {
          loop_state.fade_on_next_transition = false;
          loop_state.skip_tick_during_fade_in = true;
          renderer.start_fade_in(state.fade_duration_frames());
        }
      }
    }

    // If we hit the catch-up cap, drop the backlog rather than replaying it
    // forever.
    if (tick_accumulator_ms >= TICK_MS) {
      tick_accumulator_ms = 0;
    }

    if (loop_state.screenshot_requested) {
      renderer.with_current_frame(
          [](SDL_Surface* surface) { ::save_screenshot(surface); });
      loop_state.screenshot_requested = false;
    }

    // For software cursor:
    cursor_manager_->cursor_position(last_mouse_pos_);
    renderer.render();

    SDL_SetWindowTitle(
        sdl_.window.get(),
        std::string(compat::format("Pizza Legacy {} [{} fps]",
                                   game_meta::VERSION, fps.current_fps()))
            .c_str());

    uint64_t end_wall_ms = SDL_GetTicks();
    fps.end_frame_and_sleep(end_wall_ms);
  }
  // Make sure to reset view which might use GameState in their destructor:
  current_view_.reset();
}

ScreenPoint Game::global_cursor_pos_clamped() const {
  // Source of truth for the software cursor position. We deliberately ignore
  // the motion event's own position: on macOS/SDL3 a motion event during a
  // window-boundary crossing sometimes has a garbage absolute position that
  // teleports the cursor into the middle of the screen.
  // SDL_GetGlobalMouseState is correct, so we map it through SDL's renderer
  // transform and clamp it to the logical render area.
  float global_x = 0.0f;
  float global_y = 0.0f;
  SDL_GetGlobalMouseState(&global_x, &global_y);

  int win_x = 0;
  int win_y = 0;
  SDL_GetWindowPosition(sdl_.window.get(), &win_x, &win_y);

  float render_x = 0.0f;
  float render_y = 0.0f;
  SDL_RenderCoordinatesFromWindow(
      sdl_.renderer.get(), global_x - static_cast<float>(win_x),
      global_y - static_cast<float>(win_y), &render_x, &render_y);

  return ScreenPoint{
      std::clamp(static_cast<int>(render_x), 0,
                 DisplayConstants::SCREEN_WIDTH - 1),
      std::clamp(static_cast<int>(render_y), 0, display_height_ - 1)};
}

// Only apply the workaround for that macOS bug on macOS
#if defined(__APPLE__)
constexpr bool USE_GLOBAL_CURSOR_POS = false;
#else
constexpr bool USE_GLOBAL_CURSOR_POS = false;
#endif

ScreenPoint
Game::resolve_motion_position(const SDL_MouseMotionEvent& motion) const {
  if constexpr (USE_GLOBAL_CURSOR_POS) {
    return global_cursor_pos_clamped();
  }
  else {
    return ScreenPoint{
        std::clamp(static_cast<int>(motion.x), 0,
                   DisplayConstants::SCREEN_WIDTH - 1),
        std::clamp(static_cast<int>(motion.y), 0, display_height_ - 1)};
  }
}

void Game::handle_events(LoopState& loop_state) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    SDL_ConvertEventToRenderCoordinates(sdl_.renderer.get(), &event);

    if (event.type == SDL_EVENT_QUIT) {
      loop_state.game_state.request_exit();
      break;
    }
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
      if (cursor_manager_->relative_mouse_mode()) {
        // In relative mode the cursor is locked; accumulate relative deltas so
        // the interaction system keeps receiving hover events at a virtual
        // position that moves with the physical mouse.
        last_mouse_pos_.x += static_cast<int>(event.motion.xrel);
        last_mouse_pos_.y += static_cast<int>(event.motion.yrel);
      }
      else {
        last_mouse_pos_ = resolve_motion_position(event.motion);
      }
    }
    else {
      if (event.type == SDL_EVENT_MOUSE_WHEEL &&
          loop_state.game_state.options().scroll_zoom_inverted) {
        event.wheel.y = -event.wheel.y;
      }
      interaction_manager_->process(event);
      input_manager_->process(event);
    }

    if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
      handle_resize_event(event, loop_state);
    }
    else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
      // If focus is lost mid-drag we won't get a ButtonUp, so
      // end any active drag now.
      interaction_manager_->end_active_drag();
    }
  }
  // A drag that ended this frame unlocked the pointer during event processing
  // above. Restore the position to the one before we entered relative mouse
  // mode, so that when the cursor becomes visible it appears in the same
  // position from before the relative mode.
  if (!cursor_manager_->relative_mouse_mode() && prev_relative_mouse_mode_) {
    last_mouse_pos_ = pre_relative_mouse_pos_;
  }

  interaction_manager_->update_mouse(last_mouse_pos_);

  // If relative mouse mode just got turned on, remember where we are.
  const bool now_relative = cursor_manager_->relative_mouse_mode();
  if (now_relative && !prev_relative_mouse_mode_) {
    pre_relative_mouse_pos_ = last_mouse_pos_;
  }
  prev_relative_mouse_mode_ = now_relative;
}

void Game::handle_resize_event(const SDL_Event& event, LoopState& loop_state) {
  int new_width = event.window.data1;
  int new_height = event.window.data2;

  // In fullscreen, aspect ratio enforcement is not needed due to letterboxing.
  if (!loop_state.fullscreen) {
    if (!enforce_aspect_ratio(sdl_.window.get(), new_width, new_height)) {
      // Skip further handling as a new SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED will
      // be triggered
      return;
    }
  }
  loop_state.game_state.window_size(new_width, new_height);
  loop_state.context.cursor->update_scaled_cursor(new_width, new_height);
}

bool Game::init(int display_height, ConfigManager& config_manager) {
  display_height_ = display_height;

  // Logger that links to stdout/stderr
  Logger::add_sink([](LogLevel lvl, const std::string& msg) {
    const std::string full = Logger::level_prefix(lvl) + msg + "\n";
    if (lvl == LogLevel::Warning) {
      std::cerr << full;
    }
    else {
      std::cout << full;
    }
  });

  // Validate display height is one of the supported values
  assert(display_height_ == DisplayConstants::LOGICAL_HEIGHT ||
         display_height_ == DisplayConstants::CRT_DISPLAY_HEIGHT);

  // Seed the LCG from wall-clock time so each run differs.
  engine::random::seed(static_cast<uint32_t>(time(nullptr)));

  // Set ConfigManager instance for global access (used by Platform and
  // TextEncoding)
  ConfigManager::set_instance(&config_manager);
  platform::init_asset_format();

  bob_manager_ = std::make_unique<BobManager>(BobLoader());
  font_manager_ = std::make_unique<FontManager>(FontLoader());
  vga_manager_ = std::make_unique<VgaManager>(VgaLoader());
  blitmap_manager_ = std::make_unique<BlitmapManager>(vga_manager_.get());
  cursor_manager_ =
      std::make_unique<CursorManager>(vga_manager_.get(), display_height);
  tooltip_manager_ = std::make_unique<TooltipManager>();
  interaction_manager_ = std::make_unique<views::ui::InteractionManager>(
      *tooltip_manager_, display_height);
  input_manager_ = std::make_unique<input::InputManager>();
  input::configure_default_bindings(*input_manager_);

  // Initialize the SDL subsystems we use on top of the bootstrap.
  if (!sdl_session_.init_subsystems(SDL_INIT_VIDEO | SDL_INIT_AUDIO |
                                    SDL_INIT_GAMEPAD)) {
    return false;
  }

  // Create audio manager; init and asset loading are deferred to Intro
  // so the Microprose logo is already visible before the slow adl_init call.
  audio_manager_ = std::make_unique<audio::AudioManager>();
  context.audio = audio_manager_.get();

  auto [win_w, win_h] = resolution_window_size(
      config_manager.options().resolution, display_height);
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer_handle = nullptr;
  SDL_CreateWindowAndRenderer("Pizza Legacy", win_w, win_h,
                              SDL_WINDOW_RESIZABLE, &window, &renderer_handle);
  sdl_.window.reset(window);
  sdl_.renderer.reset(renderer_handle);
  SDL_SetWindowPosition(sdl_.window.get(), SDL_WINDOWPOS_CENTERED,
                        SDL_WINDOWPOS_CENTERED);

  // Bring the window to the foreground; when I launch the game from
  // Terminal.app it stays in the background otherwise.
  SDL_RaiseWindow(sdl_.window.get());

  input_manager_->window(sdl_.window.get());
  cursor_manager_->window(sdl_.window.get());

  // Set up the screen
  Logger::debug("Creating screen: w: {}, h: {}, bpp: {}",
                DisplayConstants::SCREEN_WIDTH, display_height,
                DisplayConstants::BPP);

  auto* main_surface = SDL_CreateSurface(
      DisplayConstants::SCREEN_WIDTH, display_height, SDL_PIXELFORMAT_RGBA8888);

  screen_.reset(main_surface);

  sdl_.texture.reset(create_display_texture(display_height));

  if (!screen_ || sdl_.texture == nullptr || sdl_.renderer == nullptr) {
    return false;
  }
  SDL_SetRenderLogicalPresentation(
      sdl_.renderer.get(), DisplayConstants::SCREEN_WIDTH, display_height,
      SDL_LOGICAL_PRESENTATION_LETTERBOX);

  cursor_manager_->base_cursor_mode(UiMode::Menu);

  context.bob = bob_manager_.get();
  context.font = font_manager_.get();
  context.vga = vga_manager_.get();
  context.blitmap = blitmap_manager_.get();
  context.cursor = cursor_manager_.get();
  context.interaction = interaction_manager_.get();
  context.input = input_manager_.get();
  // context.config is set in run() after ConfigManager is created

  return true;
}

namespace {
// To help figure out palette for popups and mouse cursor
bool is_menu_screen(ScreenTag::Kind kind) {
  using K = ScreenTag::Kind;
  switch (kind) {
  case K::Intro:
  case K::MenuMain:
  case K::MenuNewGame:
  case K::MenuOptions:
  case K::MenuQuickstart:
  case K::MenuHighScores:
  case K::CharacterSelection:
  case K::DifficultySelection:
  case K::DiskMenu:
  case K::MenuGameLoad:
  case K::MenuSave:
  case K::MenuSaveAsGame:
    return true;
  default:
    return false;
  }
}
} // namespace

Renderable* Game::change_state(GameState& gs, LoopState& /*loop*/) {
  current_view_.reset();

  // Default to game; menu screens below flip this. Applied to the cursor and
  // popups once after we know which screen we are building.
  UiMode ui_mode = UiMode::Game;
  std::visit(
      overloaded{

          [&](const ScreenTag& s) {
            using K = ScreenTag::Kind;
            if (is_menu_screen(s.kind)) {
              ui_mode = UiMode::Menu;
            }
            switch (s.kind) {
            case K::Intro:
              current_view_ = std::make_unique<Intro>(context, gs);
              break;
            case K::MenuMain:
              current_view_ = std::make_unique<MenuMain>(context, gs);
              break;
            case K::MenuHighScores:
              current_view_ = std::make_unique<MenuHighScores>(context, gs);
              break;
            case K::MenuNewGame:
              current_view_ = std::make_unique<MenuNewGame>(context, gs);
              break;
            case K::MenuOptions:
              current_view_ = std::make_unique<MenuOptions>(context, gs);
              break;
            case K::MenuQuickstart:
              current_view_ = std::make_unique<MenuQuickstart>(context, gs);
              break;
            case K::CharacterSelection:
              current_view_ = std::make_unique<CharacterSelection>(context, gs);
              break;
            case K::DifficultySelection:
              current_view_ =
                  std::make_unique<DifficultySelection>(context, gs);
              break;
            case K::DiskMenu:
              current_view_ = std::make_unique<DiskMenu>(context, gs);
              break;
            case K::MenuGameLoad:
              current_view_ = std::make_unique<MenuGameLoad>(context, gs);
              break;
            case K::MenuSave:
              current_view_ = std::make_unique<MenuSave>(context, gs);
              break;
            case K::MenuSaveAsGame:
              current_view_ = std::make_unique<MenuSaveAsGame>(context, gs);
              break;
            case K::Advertising:
              current_view_ = std::make_unique<Advertising>(context, gs);
              break;
            case K::StaffView:
              current_view_ = std::make_unique<StaffView>(context, gs);
              break;
            case K::Newspaper:
              current_view_ = std::make_unique<Newspaper>(context, gs);
              break;
            case K::GlobalStats:
              current_view_ = std::make_unique<GlobalStats>(context, gs);
              break;
            }
          },

          // data-bearing screens
          [&](const MarketResearchState& mr) {
            current_view_ =
                std::make_unique<MarketResearch>(context, gs, mr.district_id);
          },
          [&](const EuroMapState& em) {
            current_view_ =
                std::make_unique<EuroMap>(context, gs, em.mode, em.category);
          },
          [&](const RestaurantViewState& mm) {
            current_view_ = std::make_unique<RestaurantView>(context, gs, mm);
          },
          [&](const CityViewState& cm) {
            current_view_ = std::make_unique<CityView>(context, gs, cm.city_id);
          },
          [&](const PizzaJudgesState& pj) {
            current_view_ = std::make_unique<PizzaJudges>(
                context, gs, pj.scores, pj.on_complete, pj.judging_mode);
          },
          [&](const CategoryOverviewState& os) {
            current_view_ =
                std::make_unique<CategoryOverviewScreen>(context, gs, os.type);
          },
          [&](const IngredientCategoryViewState& is) {
            current_view_ = std::make_unique<IngredientCategoryView>(
                context, gs, is.category);
          },
          [&](const DepotIngredientCategoryViewState& ds) {
            current_view_ = std::make_unique<DepotIngredientCategoryView>(
                context, gs, ds.category);
          },
          [&](const DepotMainState& dms) {
            current_view_ = std::make_unique<views::screens::depot::DepotMain>(
                context, gs, dms);
          },
          [&](const DepotExportState& des) {
            current_view_ = std::make_unique<DepotExportScreen>(
                context, gs, des.category, des.target_city_id,
                des.transport_cost);
          },
          [&](const DepotWeaponInterfaceState& dwis) {
            current_view_ =
                std::make_unique<views::screens::depot::DepotWeaponInterface>(
                    context, gs, dwis);
          },
          [&](const StockMainState& /*sms*/) {
            current_view_ =
                std::make_unique<views::screens::stock::StockMain>(context, gs);
          },
          [&](const ContractsViewState& /*cvs*/) {
            current_view_ = std::make_unique<Contracts>(context, gs);
          },
          [&](const IngredientSupplierListState& isls) {
            current_view_ = std::make_unique<IngredientSupplierList>(
                context, gs, isls.category);
          },
          [&](const DepotIngredientSupplierListState& ds) {
            current_view_ = std::make_unique<DepotIngredientSupplierList>(
                context, gs, ds.category, ds.mode);
          },
          [&](const IngredientOrderState& ios) {
            current_view_ = std::make_unique<IngredientOrderScreen>(
                context, gs, ios.supplier_id, ios.category,
                ios.saved_ingredients);
          },
          [&](const DepotIngredientOrderState& ds) {
            current_view_ =
                std::make_unique<DepotIngredientOrderScreen>(context, gs, ds);
          },
          [&](const SupplierContractState& scs) {
            // Find the supplier from the city
            auto* city = gs.active_city();
            ActorType supplier_type =
                core::food::CategoryRegistry::supplier_type(scs.category);
            const FoodSupplier* supplier = nullptr;
            for (const auto* s : city->food_suppliers(supplier_type)) {
              if (s->id() == scs.supplier_id) {
                supplier = s;
                break;
              }
            }
            if (supplier) {
              auto conversation = std::make_unique<SupplierConversation>(
                  gs, supplier, scs.order, scs.total);
              auto* conv_ptr = conversation.get();
              // On success, pop IngredientOrderScreen too so we don't return
              // to it after the contract is signed.
              conversation->on_end_ = [&gs, conv_ptr]() {
                if (conv_ptr->contract_accepted()) {
                  gs.ui().pop();
                }
              };
              Actor* actor_ptr = conversation->owned_actor();
              current_view_ = std::make_unique<ActorView<SupplierLine>>(
                  context, gs, actor_ptr, std::move(conversation));
            }
          },
          [&](const FurnitureListState& fs) {
            current_view_ = std::make_unique<FurnitureListView>(context, gs,
                                                                fs.supplier_id);
          },
          [&](const FloorListState& fs) {
            current_view_ =
                std::make_unique<FloorListView>(context, gs, fs.supplier_id);
          },
          [&](const PurchasingState& /*ps*/) {
            current_view_ = std::make_unique<PurchasingMain>(context, gs);
          },
          [&](const SupplierListState& ss) {
            current_view_ =
                std::make_unique<SupplierList>(context, gs, ss.supplier);
          },
          [&](const PurchasingDemographicState& /*ps*/) {
            current_view_ =
                std::make_unique<views::screens::purchasing::Demographics>(
                    context, gs);
          },
          [&](const BankState& bs) {
            const auto& city = gs.active_city();
            auto* actor = city->actor_by_id(bs.id);
            auto conversation = std::make_unique<BankConversation>(
                gs, actor, city->bank_settings(actor->id()));
            current_view_ = std::make_unique<ActorView<BankLine>>(
                context, gs, actor, std::move(conversation));
          },
          [&](const PoliceState& ps) {
            auto* actor = gs.active_city()->actor_by_id(ps.id);
            auto conversation =
                std::make_unique<PoliceConversation>(gs, actor, ps.reason);
            auto view = std::make_unique<ActorView<PoliceLine>>(
                context, gs, actor, std::move(conversation));
            view->set_on_complete(ps.on_complete);
            current_view_ = std::move(view);
          },
          [&](const MafiaState& ms) {
            auto* actor = gs.active_city()->actor_by_id(ms.id);
            auto mode = ms.mode.value_or(MafiaMode::PlayerCall);
            auto conversation = std::make_unique<MafiaConversation>(
                gs, actor, mode, ms.restaurant_id, ms.first_contact);
            current_view_ = std::make_unique<ActorView<MafiaLine>>(
                context, gs, actor, std::move(conversation));
          },
          [&](const MayorState& mys) {
            auto* city = gs.active_city();
            auto* actor = city->actor_by_id(mys.id);
            std::optional<MayorConversation::InspectionOutcome> opt_outcome;
            if (mys.is_first_inspection.has_value()) {
              opt_outcome = MayorConversation::InspectionOutcome{
                  .star_gained = mys.inspection_star_gained,
                  .star_lost = mys.inspection_star_lost,
                  .is_first = *mys.is_first_inspection,
              };
            }
            std::string restaurant_name;
            if (mys.restaurant_id.has_value()) {
              if (auto* r = gs.restaurant(*mys.restaurant_id)) {
                restaurant_name = r->name();
              }
            }
            auto conversation = std::make_unique<MayorConversation>(
                gs, actor, city->mayor_office(), mys.type, opt_outcome,
                mys.award_winner_player_id, restaurant_name);
            if (mys.on_complete) {
              conversation->on_end_ = mys.on_complete;
            }
            current_view_ = std::make_unique<ActorView<MayorLine>>(
                context, gs, actor, std::move(conversation));
          },
          [&](const TerminalState& ts) {
            current_view_ = std::make_unique<Terminal>(context, gs, ts);
          },
          [&](const ActorTerminalState& at) {
            current_view_ = ActorViewFactory::create(context, gs, at);
          },

          [&](const GameOverState& gos) {
            current_view_ = std::make_unique<GameOver>(context, gs, gos);
          },
          [&](const CookbookReminderState& pps) {
            current_view_ =
                std::make_unique<CookbookReminder>(context, gs, pps);
          },
          [&](const ViewerState& v) {
            ui_mode = UiMode::Menu;
            // Use the viewer type stored in the ViewerState
            handle_viewer_state(gs, v.filename, v.type);
          },
          [&](const OptionsMenuState& oms) {
            ui_mode = UiMode::Menu;
            current_view_ =
                std::make_unique<MenuOptions>(context, gs, oms.screen);
          },
          [&](const RestaurantMenuState& rms) {
            current_view_ =
                std::make_unique<RestaurantMenu>(context, gs, rms.id);
          },
          [&](const RestaurantStatsState& rss) {
            std::optional<RestaurantStatsScreen> screen = std::nullopt;
            if (rss.screen_index.has_value()) {
              screen =
                  static_cast<RestaurantStatsScreen>(rss.screen_index.value());
            }
            current_view_ = std::make_unique<RestaurantStats>(
                context, gs, rss.id, screen, rss.automatic_mode);
          },
          [&](const WeeksTakingsState& wts) {
            current_view_ = std::make_unique<WeeksTakings>(
                context, gs, wts.id, wts.end_of_week_mode, wts.on_done);
          },
          [&](const StatsRestaurantsState& srs) {
            current_view_ =
                std::make_unique<StatsRestaurants>(context, gs, srs.player_id);
          },
          [&](const AnimatedStatsState& ass) {
            current_view_ = std::make_unique<AnimatedStats>(context, gs, ass.id,
                                                            ass.screen);
          },
          [&](const PizzaDesignState& pms) {
            current_view_ = std::make_unique<PizzaDesign>(context, gs, pms);
          },
          [&](const PlayerTurnCardState& ptcs) {
            current_view_ = std::make_unique<PlayerTurnCard>(context, gs, ptcs);
          },
          [&](const FullscreenStatusState& fss) {
            current_view_ = std::make_unique<FullscreenStatus>(
                context, gs, fss.message_id, fss.on_dismiss);
          },
          [&](const WinSequenceState& wss) {
            current_view_ = std::make_unique<WinSequenceView>(
                context, gs, wss.line_index, wss.player_id);
          },
          [&](const MafiaMissionViewState& mmvs) {
            current_view_ =
                std::make_unique<MafiaMissionView>(context, gs, mmvs.line);
          },
          [&](const PlayerProfileState& pps) {
            current_view_ =
                std::make_unique<PlayerProfile>(context, gs, pps.player_id);
          },
          // no screen
          [&](const std::monostate&) { /* leave current_view_ == nullptr */ }

      },
      gs.ui().top());

  // Apply the resolved mode to everything that follows the menu/game look.
  cursor_manager_->base_cursor_mode(ui_mode);
  context.ui_mode = ui_mode;

  // Wrap in PaneledView if necessary
  if (current_view_ && !dynamic_cast<PaneledView*>(current_view_.get())) {
    if (dynamic_cast<MainPanelView*>(current_view_.get())) {
      current_view_ =
          std::make_unique<PaneledView>(context, gs, std::move(current_view_));
    }
  }

  return current_view_.get();
}

void Game::handle_viewer_state(GameState& state,
                               const std::filesystem::path& filename,
                               ViewerType type) {
  if (filename.empty()) {
    return;
  }
  // fonts and pal are not actually filenames but just tell us that we want
  // to view fonts or the palette. We'll deal with proper arguments maybe
  // in the future since it's just for debugging.
  if (filename == "fonts") {
    current_view_ = std::make_unique<FontViewer>(context, state);
    return;
  }
  if (filename == "sounds") {
    current_view_ = std::make_unique<SoundViewer>(context, state);
    return;
  }
  if (filename == "pal") {
    current_view_ = std::make_unique<PaletteView>(context, state);
    return;
  }
  if (filename == "char") {
    current_view_ = std::make_unique<CharacterViewer>(context, state);
    return;
  }

  // If explicit type is provided, use it; otherwise auto-detect
  ViewerType effective_type = type;
  if (effective_type == ViewerType::Auto) {
    if (!fileutils::fexists(filename.string())) {
      std::cerr << "File not found: " << filename << "\n";
      exit(1);
    }
    std::string ext = filename.extension().string();
    if (ext == ".vga") {
      effective_type = ViewerType::Vga;
    }
    else if (ext == ".bob") {
      effective_type = ViewerType::Bob;
    }
    else {
      effective_type = ViewerType::Vga;
      std::cerr << "Unknown file extension: " << ext << ", assuming VGA\n";
    }
  }

  // Use open_viewer for the actual loading with the determined type
  open_viewer(state, filename, effective_type);
}

void Game::open_viewer(GameState& state, const std::filesystem::path& path,
                       ViewerType type) {
  // Check if file exists
  if (!std::filesystem::exists(path)) {
    std::cerr << "File not found: " << path << "\n";
    exit(1);
  }

  try {
    switch (type) {
    case ViewerType::Vga:
      Logger::info("Loading VGA file: {}", path.string());
      current_view_ =
          std::make_unique<VgaViewer>(context, state, path.string());
      Logger::info("VGA viewer loaded successfully");
      break;
    case ViewerType::Bob:
      Logger::info("Loading BOB file: {}", path.string());
      current_view_ =
          std::make_unique<BobViewer>(context, state, path.string());
      Logger::info("BOB viewer loaded successfully");
      break;
    case ViewerType::Auto:
      // Fallback to extension-based detection
      handle_viewer_state(state, path);
      break;
    }
  }
  catch (const std::exception& e) {
    Logger::warn("Failed to load viewer: {}", e.what());
    std::cerr << "ERROR: Failed to load file: " << path << "\n";
    std::cerr << "Details: " << e.what() << "\n";
    exit(1);
  }
  catch (...) {
    Logger::warn("Unknown error loading viewer for: {}", path.string());
    std::cerr << "ERROR: Unknown error loading file: " << path << "\n";
    exit(1);
  }
}

// Returns true if the window size was let through without issue,
// and false when there will be another resize event coming
bool Game::enforce_aspect_ratio(SDL_Window* window, int new_width,
                                int new_height) {
  // Calculate the target aspect ratio in reduced form
  const int gcd = std::gcd(DisplayConstants::SCREEN_WIDTH, display_height_);
  const int target_width_ratio = DisplayConstants::SCREEN_WIDTH / gcd;
  const int target_height_ratio = display_height_ / gcd;
  // Logger::debug("TWR: {}, THR: {}", target_width_ratio, target_height_ratio);

  // Calculate the current aspect ratio
  int corrected_width = new_width;
  int corrected_height = new_height;

  // Determine which dimension needs correction
  if (new_width * target_height_ratio > new_height * target_width_ratio) {
    Logger::debug("{} * {} ({}) >  {} * {} ({})", new_width,
                  target_height_ratio, new_width * target_height_ratio,
                  new_height, target_width_ratio,
                  new_height * target_width_ratio);
    // Too wide: adjust the width
    corrected_width = new_height * target_width_ratio / target_height_ratio;
    Logger::debug("Adjusting width from {} to {}", new_width, corrected_width);
  }
  else if (new_width * target_height_ratio < new_height * target_width_ratio) {
    Logger::debug("{} * {} ({}) <  {} * {} ({})", new_width,
                  target_height_ratio, new_width * target_height_ratio,
                  new_height, target_width_ratio,
                  new_height * target_width_ratio);
    // Too tall: adjust the height
    corrected_height = new_width * target_height_ratio / target_width_ratio;
    Logger::debug("Adjusting height from {} to {}", new_height,
                  corrected_height);
  }

  // Only set the window size if it needs to be corrected
  if (corrected_width != new_width || corrected_height != new_height) {
    if (!window_resizing_in_progress_) {
      window_resizing_in_progress_ = true;
    }
    else {
      // If we've just sent a window resize event but still aren't quite there
      // we're just going to pretend it's fine now, since we don't want to keep
      // trying to resize by a few pixels (that doesn't happen luckily, but it
      // also means I can't use the return value of this function to determine
      // if I can expect another resize event or not. So, just return true if
      // we've just sent a previous request.
      window_resizing_in_progress_ = false;
      return true;
    }
    Logger::debug("Correcting size!");
    SDL_SetWindowSize(window, corrected_width, corrected_height);
    return false;
  }
  window_resizing_in_progress_ = false;
  return true;
}

/* Just for debugging for if you skip player selections screen */
void Game::create_some_players(GameState& state) {
  std::array<std::unique_ptr<Player>, NUMBER_OF_PLAYERS> players;
  for (int i = 0; i < NUMBER_OF_PLAYERS; i++) {
    players[i] = std::make_unique<Player>(
        core::PlayerId{i},
        core::character::Character(CharacterFactory::create_character(i)));
    players[i]->name(std::string("Player " + std::to_string(i)));
  }
  state.players(std::move(players));
}

int Game::get_display_height(bool crt_mode) {
  const int height = crt_mode ? DisplayConstants::CRT_DISPLAY_HEIGHT
                              : DisplayConstants::LOGICAL_HEIGHT;
  assert(height == DisplayConstants::LOGICAL_HEIGHT ||
         height == DisplayConstants::CRT_DISPLAY_HEIGHT);
  return height;
}

SDL_Texture* Game::create_display_texture(int display_height) const {
  return SDL_CreateTexture(sdl_.renderer.get(), SDL_PIXELFORMAT_RGBA8888,
                           SDL_TEXTUREACCESS_STREAMING,
                           DisplayConstants::SCREEN_WIDTH, display_height);
}
