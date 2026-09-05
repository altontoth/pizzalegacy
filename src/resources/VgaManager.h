/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "engine/gfx/Surface.h"
#include "resources/ResourceManager.h"
#include "resources/VgaLoader.h"

using gfx::Surface;

using VgaManager = ResourceManager<Surface, VgaLoader, false>;
