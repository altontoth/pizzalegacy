/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/state/CompetitionCoordinator.h"

#include <algorithm>
#include <string>
#include <utility>

#include "core/character/Actor.h"
#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/events/MilestoneId.h"
#include "core/finance/Ledger.h"
#include "core/finance/Money.h"
#include "core/food/Pizza.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/util/Constants.h"
#include "core/util/GameContext.h"
#include "engine/Logger.h"
#include "engine/Random.h"
#include "factories/PizzaFactory.h"
#include "resources/BobSizeFn.h"

void CompetitionCoordinator::run_monthly_competition() {
  std::vector<core::PlayerId> humans;
  for (int slot = 0; slot < gs_->human_player_count(); ++slot) {
    auto* p = gs_->players()[slot];
    humans.push_back(p->id());
  }
  run_monthly_competition_chain(std::move(humans),
                                std::make_shared<CompetitionSession>());
}

void CompetitionCoordinator::run_monthly_competition_chain(
    std::vector<core::PlayerId> remaining,
    std::shared_ptr<CompetitionSession> session) {
  // Remove players from the front until we find the first player than can
  // afford it
  while (!remaining.empty()) {
    auto* p = gs_->player(remaining.front());
    if (p && p->cash_balance() >= 500_usd) {
      break;
    }
    remaining.erase(remaining.begin());
  }

  if (remaining.empty()) {
    launch_monthly_competition(std::move(session));
    return;
  }

  core::PlayerId pid = remaining.front();
  remaining.erase(remaining.begin());

  if (!gs_->monthly_entry_presenter_) {
    // Headless (no UI registered): nobody can be asked, skip this player.
    run_monthly_competition_chain(std::move(remaining), std::move(session));
    return;
  }

  // The first accepting player also picks the speed.
  const bool ask_speed = !session->canonical_pizza;
  gs_->monthly_entry_presenter_(
      pid, ask_speed,
      [this, pid, remaining = std::move(remaining),
       session](std::optional<MonthlyCompetitionEntry> entry) mutable {
        if (!entry) {
          run_monthly_competition_chain(std::move(remaining),
                                        std::move(session));
          return;
        }

        if (!session->canonical_pizza) {
          int speed_level = entry->speed_level.value_or(3);
          int pizza_index = std::min(competition_count_, 40) +
                            engine::random::rand_max_n(10) + 20;
          auto& zutaten_bob = gs_->context_->bob->get("bob/zutaten.bob");
          auto get_bob_size =
              make_ingredient_bob_size_fn<Bob, core::food::Topping>(
                  zutaten_bob);
          auto canonical = PizzaFactory::create_competition_pizza(
              pizza_index, get_bob_size, gs_->customer_taste_profiles());
          if (!canonical) {
            Logger::warn("run_monthly_competition: failed to load pizza {}",
                         pizza_index);
            run_monthly_competition_chain(std::move(remaining),
                                          std::move(session));
            return;
          }
          session->pizza_index = pizza_index;
          session->speed_level = speed_level;
          session->delay_ticks = 100 - (speed_level * 15);
          session->canonical_pizza =
              std::make_shared<core::food::Pizza>(std::move(*canonical));
        }

        session->participants.push_back(CompetitionParticipant{
            .player_id = pid,
            .is_human = true,
            .bet = entry->bet,
        });
        run_monthly_competition_chain(std::move(remaining), std::move(session));
      });
}

void CompetitionCoordinator::launch_monthly_competition(
    std::shared_ptr<CompetitionSession> session) {
  // Shouldn't happen
  if (!session->canonical_pizza) {
    gs_->turn().advance_pipeline();
    return;
  }

  // use the bet from the first human participant
  Money ai_bet =
      session->participants.empty() ? 500_usd : session->participants[0].bet;

  // Add AI opponents that aren't already in the session.
  for (int slot = 0; slot < NUMBER_OF_PLAYERS; ++slot) {
    auto* ai = gs_->players()[slot];
    if (!ai || ai->is_human_controlled()) {
      continue;
    }
    session->participants.push_back(CompetitionParticipant{
        .player_id = ai->id(),
        .is_human = false,
        .bet = ai_bet,
    });
  }

  if (session->participants.empty()) {
    gs_->turn().advance_pipeline();
    return;
  }

  // human_slot points to the first human in the list (index 0, since humans
  // were pushed before AIs).
  session->human_slot = 0;
  core::PlayerId first_pid = session->participants[0].player_id;

  collect_competition_bets(*session);

  gs_->ui().push<PizzaDesignState>(PizzaDesignState{
      .initial_mode = PizzaDesignMode::MonthlyChallenge,
      .competition_session = std::move(session),
      .competing_player_id = first_pid,
      .on_done = [this] { gs_->turn().advance_pipeline(); },
  });
}

void CompetitionCoordinator::collect_competition_bets(
    const CompetitionSession& session) {
  GameDate today = gs_->time().current_date();
  for (const auto& p : session.participants) {
    auto* participant = gs_->player(p.player_id);
    if (participant) {
      participant->financials().spend_cash(today, Account::Others, p.bet);
    }
  }
}

std::optional<core::events::MilestoneId>
CompetitionCoordinator::finalize_competition_results(
    CompetitionSession& session) {
  auto& c = session;

  // Simulate AI participants
  for (auto& p : c.participants) {
    if (p.is_human) {
      continue;
    }
    const auto* ai = gs_->player(p.player_id);
    if (!ai) {
      continue;
    }
    const auto& ch = ai->character();
    auto dex = ch.dexterity();
    auto cook = ch.cooking();
    auto raw_time = 50 - engine::random::rand_max_n((dex / 2) + 1);
    auto time_ticks = raw_time + (raw_time * competition_count_ * 5 / 100);
    auto difficulty = c.pizza_index - 10 + (c.speed_level * 5);
    int q = 100 - difficulty + engine::random::rand_max_n(14) +
            (static_cast<int>(cook) / 5);
    int match_score = std::clamp(q, 0, 99);
    int speed_bonus = match_score * (c.speed_level * 20) / 100;
    int final_score =
        std::clamp(match_score + speed_bonus - (time_ticks / 3), 0, 200);
    p.match_score = match_score;
    p.time_ticks = time_ticks;
    p.final_score = final_score;
  }

  // Determine winner
  int best = -1;
  c.winner_slot = -1;
  c.all_tied = true;
  for (int i = 0; i < static_cast<int>(c.participants.size()); ++i) {
    int score = c.participants[i].final_score;
    if (score > best) {
      best = score;
      c.winner_slot = i;
    }
    if (i > 0 &&
        c.participants[i].final_score != c.participants[i - 1].final_score) {
      c.all_tied = false;
    }
  }
  if (c.participants.size() < 2) {
    c.all_tied = false;
  }

  // The event for a winning human, delivered by the caller once the results
  // screen is dismissed, don't want to spoil the results :)
  std::optional<core::events::MilestoneId> winner_message;

  // Distribute prize money and update counters
  GameDate today = gs_->time().current_date();
  if (c.winner_slot >= 0 && !c.all_tied) {
    auto& winner = c.participants[c.winner_slot];
    auto* winner_player = gs_->player(winner.player_id);

    // You can only win what you bet.
    Money winnings = winner.bet * 2;

    if (winner_player) {
      winner_player->financials().transfer(today, Account::Sales, Account::Cash,
                                           winnings,
                                           AccountingScope::Operational);
      gs_->adjust_social_score(*winner_player, 40);
    }

    // Win counter
    if (winner_player) {
      winner_player->increment_competition_win_count();
      uint8_t wins = winner_player->competition_win_count();

      if (winner.is_human) {
        if (wins == 1) {
          winner_message =
              core::events::MilestoneId::FirstPriceInPizzaCompetition;
        }
        else if (wins == 10) {
          winner_message = core::events::MilestoneId::Won10PizzaCompetitions;
        }
      }
    }

    // Challenge mode: an AI that loses gets a cool down before wanting to be
    // challenged again
    if (c.is_challenge && c.human_slot >= 0 && c.winner_slot == c.human_slot) {
      for (auto& p : c.participants) {
        auto* ai = gs_->player(p.player_id);
        if (ai && !ai->is_human_controlled()) {
          uint8_t courage = ai->character().courage();
          uint8_t penalty =
              static_cast<uint8_t>(engine::random::rand_max_n(100 - courage));
          ai->set_competition_willingness(penalty);
        }
      }
    }
  }
  else {
    // Draw: refund all bets
    for (auto& p : c.participants) {
      auto* participant = gs_->player(p.player_id);
      if (participant) {
        participant->financials().transfer(today, Account::Others,
                                           Account::Cash, p.bet,
                                           AccountingScope::Operational);
      }
    }
  }

  increment_competition_count();

  return winner_message;
}

bool CompetitionCoordinator::run_annual_pizza_competition() {
  auto* city = gs_->active_city();
  if (!city) {
    return false;
  }
  auto mayor_actors = city->actors(ActorType::MAYOR);
  if (mayor_actors.empty()) {
    return false;
  }
  int mayor_id = mayor_actors[0]->id();

  auto session = std::make_shared<CompetitionSession>();

  // Add all human players first so their design screens run sequentially
  // before the winner is announced.
  for (int slot = 0; slot < NUMBER_OF_PLAYERS; ++slot) {
    auto* hp = gs_->players()[slot];
    if (hp && hp->is_human_controlled()) {
      session->participants.push_back(
          CompetitionParticipant{.player_id = hp->id(), .is_human = true});
    }
  }

  if (session->participants.empty()) {
    return false;
  }

  // human_slot starts at the first human (index 0).
  session->human_slot = 0;

  // AI scores are pre-computed from character stats.
  for (int slot = 0; slot < NUMBER_OF_PLAYERS; ++slot) {
    auto* ai = gs_->players()[slot];
    if (!ai || ai->is_human_controlled()) {
      continue;
    }
    int score = std::clamp(static_cast<int>(ai->character().cooking()) +
                               engine::random::rand_max_n(20) - 10,
                           0, 100);
    session->participants.push_back(CompetitionParticipant{
        .player_id = ai->id(),
        .is_human = false,
        .match_score = score,
        .final_score = score,
    });
  }

  annual_comp_.session = session;
  annual_comp_.phase = AnnualCompetitionFlow::Phase::Announcing;

  gs_->ui().push<MayorState>(MayorState{
      .id = mayor_id,
      .type = MayorConversationType::PizzaOfTheYearCompetition,
  });
  return true;
}

void CompetitionCoordinator::finalize_annual_pizza_competition(
    CompetitionSession session) {
  if (annual_comp_.phase != AnnualCompetitionFlow::Phase::AwaitingDesign) {
    return;
  }
  annual_comp_.session =
      std::make_shared<CompetitionSession>(std::move(session));
  advance_annual_competition();
}

bool CompetitionCoordinator::annual_comp_ready_to_advance() const {
  using Phase = AnnualCompetitionFlow::Phase;
  switch (annual_comp_.phase) {
  case Phase::Announcing:
  case Phase::ShowingWinner:
    return !gs_->ui().is_top<MayorState>();
  default:
    return false;
  }
}

void CompetitionCoordinator::tick() {
  if (annual_comp_ready_to_advance()) {
    advance_annual_competition();
  }
}

void CompetitionCoordinator::advance_annual_competition() {
  using Phase = AnnualCompetitionFlow::Phase;
  auto& ac = annual_comp_;

  switch (ac.phase) {
  case Phase::Announcing: {
    // Mayor announcement closed, start first human's design screen.
    core::PlayerId first_pid =
        ac.session->participants[ac.session->human_slot].player_id;
    gs_->ui().push<PizzaDesignState>(PizzaDesignState{
        .initial_mode = PizzaDesignMode::AnnualCompetitionDesign,
        .competition_session = ac.session,
        .competing_player_id = first_pid,
    });
    ac.phase = Phase::AwaitingDesign;
    break;
  }

  case Phase::AwaitingDesign: {
    // Called from finalize_annual_pizza_competition when a human finishes.
    int next_slot = ac.session->human_slot + 1;
    if (next_slot < static_cast<int>(ac.session->participants.size()) &&
        ac.session->participants[next_slot].is_human) {
      // More human players, go to the next design screen.
      ac.session->human_slot = next_slot;
      core::PlayerId next_pid = ac.session->participants[next_slot].player_id;
      gs_->ui().push<PizzaDesignState>(PizzaDesignState{
          .initial_mode = PizzaDesignMode::AnnualCompetitionDesign,
          .competition_session = ac.session,
          .competing_player_id = next_pid,
      });
      // Stay in AwaitingDesign.
      break;
    }

    // All human players done, figure out winner and let Mayor announce
    auto* city = gs_->active_city();
    if (!city) {
      ac.phase = Phase::Inactive;
      break;
    }
    auto mayor_actors = city->actors(ActorType::MAYOR);
    if (mayor_actors.empty()) {
      ac.phase = Phase::Inactive;
      break;
    }
    int mayor_id = mayor_actors[0]->id();

    int best = -1;
    int winner_slot = -1;
    bool all_tied = ac.session->participants.size() >= 2;
    for (int i = 0; i < static_cast<int>(ac.session->participants.size());
         ++i) {
      int s = ac.session->participants[i].final_score;
      if (s > best) {
        best = s;
        winner_slot = i;
      }
      if (i > 0 && s != ac.session->participants[i - 1].final_score) {
        all_tied = false;
      }
    }

    Logger::debug("Pizza of the Year scores:");
    for (int i = 0; i < static_cast<int>(ac.session->participants.size());
         ++i) {
      const auto& p = ac.session->participants[i];
      auto* pl = gs_->player(p.player_id);
      std::string pname = pl ? pl->name() : "?";
      Logger::debug("  [{}] {} ({}): score={}", i, pname,
                    p.is_human ? "human" : "AI", p.final_score);
    }
    if (winner_slot >= 0 && !all_tied) {
      const auto& wp = ac.session->participants[winner_slot];
      auto* wpl = gs_->player(wp.player_id);
      Logger::debug("  => winner: {}", wpl ? wpl->name() : "?");
    }
    else {
      Logger::debug("  => all tied, no winner");
    }

    std::optional<int> winner_player_id;
    if (winner_slot >= 0 && !all_tied) {
      auto& winner_p = ac.session->participants[winner_slot];
      winner_player_id = winner_p.player_id.value;
      if (auto* winner = gs_->player(winner_p.player_id)) {
        gs_->adjust_social_score(*winner, 40);
        using core::events::MilestoneId;
        gs_->record_milestone(MilestoneId::AwardBestPizzaOfTheYear,
                              winner->id());
        gs_->record_milestone(MilestoneId::AwardRestaurantOfTheYear,
                              winner->id());
      }
    }

    gs_->ui().push<MayorState>(MayorState{
        .id = mayor_id,
        .type = MayorConversationType::PizzaOfTheYearWinner,
        .award_winner_player_id = winner_player_id,
    });
    ac.phase = Phase::ShowingWinner;
    break;
  }

  case Phase::ShowingWinner:
    // Annual competition finished. Pipeline continues to MonthlyCompetition
    // (for December)
    ac.session.reset();
    ac.phase = Phase::Inactive;
    gs_->turn().advance_pipeline();
    break;

  default:
    break;
  }
}
