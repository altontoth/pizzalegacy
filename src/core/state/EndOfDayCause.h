/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

// What ended the player's day. The clock reached their end of day, something
// the player did ended it early (the End turn button, leaving on holiday,
// being sent to jail), or they were away for it (a jail or holiday day, and
// the day they come back). Each cause has its own stat changes at the day's
// end.
enum class EndOfDayCause { Clock, PlayerAction, Away };
