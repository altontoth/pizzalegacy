/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <filesystem>
#include "resources/BobSizeFn.h"

namespace core::food {
class Pizza;
}

namespace io::recipe {
bool save_prez(const core::food::Pizza& pizza,
               const std::filesystem::path& path,
               const BobSizeFn& get_bob_size);
};
