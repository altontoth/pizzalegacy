/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/GameLocationPrompt.h"

#include <atomic>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "engine/Logger.h"
#include "engine/SDL_include.h"
#include "resources/util/FileUtils.h"

namespace engine {

std::string find_dos_game(const std::string& base) {
  if (base.empty()) {
    return {};
  }
  namespace fs = std::filesystem;
  fs::path b(base);

  std::vector<fs::path> candidates;
  auto add = [&](auto&& p) { candidates.push_back(b / p); };

  add(".");
  add("game");
  add("Pizza Connection");
  add("Pizza Connection/game");
  add("steamapps/common/Pizza Connection");
  add("steamapps/common/Pizza Connection/game");

#ifdef __APPLE__
  add("Pizza Connection.app/Contents/Resources/game");
  add("Contents/Resources/game");
  add("Pizza Connection/Pizza Connection.app/Contents/Resources/game");
  add("steamapps/common/Pizza Connection/Pizza "
      "Connection.app/Contents/Resources/game");
#endif

  for (const auto& candidate : candidates) {
    if (fileutils::fexists((candidate / "PT.EXE").string())) {
      return candidate.lexically_normal().string();
    }
  }
  return {};
}

// Filled in when the dialog's callback fires, which SDL may do on another
// thread. An empty path means cancelled.
struct FolderAnswer {
  std::atomic<bool> answered = false;
  bool failed = false;
  std::string error;
  std::string path;
};

namespace {

constexpr const char* no_dialog_message =
    "No folder dialog is available on this system. Pass --game-path <folder>, "
    "or put game_path into the settings file.";

// Keep the FolderAnswer alive until the asynchronous callback runs. The
// callback owns this heap-allocated shared_ptr and releases it when done.
void folder_chosen(void* userdata, const char* const* filelist, int) {
  std::unique_ptr<std::shared_ptr<FolderAnswer>> held(
      static_cast<std::shared_ptr<FolderAnswer>*>(userdata));
  FolderAnswer& answer = **held;
  if (!filelist) {
    answer.failed = true;
    answer.error = SDL_GetError();
  }
  else if (filelist[0]) {
    answer.path = filelist[0];
  }
  answer.answered.store(true, std::memory_order_release);
}

} // namespace

void FolderDialog::open() {
  if (answer_) {
    return;
  }
  answer_ = std::make_shared<FolderAnswer>();
  auto* held = new std::shared_ptr<FolderAnswer>(answer_);
  SDL_ShowOpenFolderDialog(folder_chosen, held, nullptr, nullptr, false);
}

std::optional<std::string> FolderDialog::take_choice() {
  if (!answer_ || !answer_->answered.load(std::memory_order_acquire)) {
    return std::nullopt;
  }
  const std::shared_ptr<FolderAnswer> answer = std::move(answer_);

  if (answer->failed) {
    Logger::warn("Folder dialog failed: {}", answer->error);
    std::cerr << no_dialog_message << "\n";
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", no_dialog_message,
                             nullptr);
    return std::nullopt;
  }
  if (answer->path.empty()) {
    return std::nullopt;
  }
  std::string dos_path = find_dos_game(answer->path);
  if (dos_path.empty()) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error",
                             "Could not find PT.EXE in this directory or any "
                             "known Steam/GOG subdirectory.",
                             nullptr);
    open();
    return std::nullopt;
  }
  Logger::info("Detected DOS version (PT.EXE) at: {}", dos_path);
  return dos_path;
}

} // namespace engine
