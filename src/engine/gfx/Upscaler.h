/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

namespace gfx {

// How an original picture is doubled in size.
enum class Upscaler { Double, Mmpx };

// What an upscaler sees past the edge of the picture.
enum class UpscaleEdge { Repeat, Blank };

} // namespace gfx
