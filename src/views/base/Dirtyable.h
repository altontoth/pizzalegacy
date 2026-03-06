/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

class Dirtyable {
public:
  virtual void mark_dirty() { dirty_ = true; }
  virtual bool is_dirty() const { return dirty_; }
  virtual void clean() { dirty_ = false; }

private:
  bool dirty_{true};
};
