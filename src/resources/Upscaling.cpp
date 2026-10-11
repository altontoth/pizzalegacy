/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/Upscaling.h"

namespace resources {

namespace {
Upscaling current;
} // namespace

void upscaling(Upscaling value) { current = value; }

const Upscaling& upscaling() { return current; }

} // namespace resources
