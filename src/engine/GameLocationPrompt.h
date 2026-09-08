/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <optional>
#include <string>

namespace engine {

// Where PT.EXE sits under `base`: the folder itself, or one of the Steam and
// GOG layouts underneath it. Empty when it is not there.
std::string find_dos_game(const std::string& base);

struct FolderAnswer;

// The system folder dialog. open() returns at once; poll take_choice().
class FolderDialog {
public:
  void open();
  [[nodiscard]] bool waiting() const { return answer_ != nullptr; }

  // The folder holding PT.EXE, once the player has chosen one.
  std::optional<std::string> take_choice();

private:
  std::shared_ptr<FolderAnswer> answer_;
};

} // namespace engine
