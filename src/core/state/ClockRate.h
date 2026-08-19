/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

// SecondsOnly keeps counting seconds without ever completing a ten-minute
// block, so a screen costs one block however long the player stays on it.
enum class ClockRate { Stopped, SecondsOnly, Running };
