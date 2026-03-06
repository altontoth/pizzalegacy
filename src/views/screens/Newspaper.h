/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef __OP_NEWSPAPER__
#define __OP_NEWSPAPER__

#include "core/newspaper/ArticleDatabase.h"
#include "views/base/MainPanelView.h"

class Newspaper : public MainPanelView {
public:
  Newspaper(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  const core::newspaper::Article* current_article_{nullptr};
  core::newspaper::Article resolved_article_;
};

#endif
