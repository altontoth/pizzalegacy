/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <functional>
#include <optional>

template <typename T>
using optional_ref = std::optional<std::reference_wrapper<T>>;
