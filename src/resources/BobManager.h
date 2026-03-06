/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "resources/Bob.h"
#include "resources/BobLoader.h"
#include "resources/ResourceManager.h"

using BobManager = ResourceManager<Bob, BobLoader>;
