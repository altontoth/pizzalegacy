/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

// How often a screen runs an interruption pass while a window is open.
// OnAction has no rate of its own; those screens wait for input, so one pass
// runs per key press or click.
enum class InterruptionRate { None, OnAction, Half, Full };
