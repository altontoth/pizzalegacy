/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/factories/ViewFactory.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <variant>

#include "core/conversations/BankConversation.h"
#include "core/conversations/MafiaConversation.h"
#include "core/conversations/MafiaLine.h"
#include "core/conversations/MayorConversation.h"
#include "core/conversations/PoliceConversation.h"
#include "core/conversations/SupplierConversation.h"
#include "core/food/IngredientCategory.h"
#include "core/state/GameState.h"
#include "core/state/Overloaded.h"
#include "core/state/UIState.h"
#include "core/state/ViewerType.h"
#include "core/util/GameContext.h"
#include "engine/Logger.h"
#include "views/base/PaneledView.h"
#include "views/base/View.h"
#include "views/factories/ActorViewFactory.h"
#include "views/screens/ActorView.h"
#include "views/screens/Advertising.h"
#include "views/screens/AnimatedStats.h"
#include "views/screens/EuroMap.h"
#include "views/screens/FullscreenStatus.h"
#include "views/screens/GameLocationScreen.h"
#include "views/screens/GameOver.h"
#include "views/screens/GlobalStats.h"
#include "views/screens/Intro.h"
#include "views/screens/MaestroMessage.h"
#include "views/screens/MafiaMissionView.h"
#include "views/screens/MarketResearch.h"
#include "views/screens/Newspaper.h"
#include "views/screens/PizzaJudges.h"
#include "views/screens/PlayerProfile.h"
#include "views/screens/PlayerTurnCard.h"
#include "views/screens/StaffView.h"
#include "views/screens/StatsRestaurants.h"
#include "views/screens/Terminal.h"
#include "views/screens/WinSequenceView.h"
#include "views/screens/city_view/CityView.h"
#include "views/screens/depot/DepotExportScreen.h"
#include "views/screens/depot/DepotMain.h"
#include "views/screens/depot/DepotWeaponInterface.h"
#include "views/screens/menus/CharacterSelection.h"
#include "views/screens/menus/DifficultySelection.h"
#include "views/screens/menus/DiskMenu.h"
#include "views/screens/menus/MenuBindings.h"
#include "views/screens/menus/MenuGameLoad.h"
#include "views/screens/menus/MenuHighScores.h"
#include "views/screens/menus/MenuMain.h"
#include "views/screens/menus/MenuMissionSelect.h"
#include "views/screens/menus/MenuNewGame.h"
#include "views/screens/menus/MenuOptions.h"
#include "views/screens/menus/MenuQuickstart.h"
#include "views/screens/menus/MenuSave.h"
#include "views/screens/menus/MenuSaveAsGame.h"
#include "views/screens/menus/MenuSaveAsMission.h"
#include "views/screens/pizza/PizzaDesign.h"
#include "views/screens/purchasing/CategoryOverviewScreen.h"
#include "views/screens/purchasing/FloorListView.h"
#include "views/screens/purchasing/FurnitureListView.h"
#include "views/screens/purchasing/PurchasingMain.h"
#include "views/screens/purchasing/SupplierList.h"
#include "views/screens/restaurant/RestaurantMenu.h"
#include "views/screens/restaurant/RestaurantStats.h"
#include "views/screens/restaurant/RestaurantView.h"
#include "views/screens/restaurant/WeeksTakings.h"
#include "views/screens/stock/Contracts.h"
#include "views/screens/stock/DepotIngredientCategoryView.h"
#include "views/screens/stock/DepotIngredientOrderScreen.h"
#include "views/screens/stock/DepotIngredientSupplierList.h"
#include "views/screens/stock/IngredientCategoryView.h"
#include "views/screens/stock/IngredientOrderScreen.h"
#include "views/screens/stock/IngredientSupplierList.h"
#include "views/screens/stock/StockMain.h"
#include "views/viewers/BobViewer.h"
#include "views/viewers/CardViewer.h"
#include "views/viewers/CharacterViewer.h"
#include "views/viewers/FontViewer.h"
#include "views/viewers/PaletteView.h"
#include "views/viewers/SoundViewer.h"
#include "views/viewers/VgaViewer.h"

namespace {

// To help figure out palette for popups and mouse cursor
bool is_menu_screen(ScreenTag::Kind kind) {
  using K = ScreenTag::Kind;
  switch (kind) {
  case K::Intro:
  case K::MenuMain:
  case K::MenuNewGame:
  case K::MenuOptions:
  case K::MenuBindings:
  case K::MenuQuickstart:
  case K::MenuHighScores:
  case K::CharacterSelection:
  case K::DifficultySelection:
  case K::DiskMenu:
  case K::MenuGameLoad:
  case K::MenuSave:
  case K::MenuSaveAsGame:
  case K::MenuSaveAsMission:
  case K::MissionSelect:
    return true;
  default:
    return false;
  }
}

// Create an asset viewer for a file (or one of the pseudo-names "fonts",
// "sounds", "pal", "char"). These are developer tools launched from the CLI;
// a bad path exits instead of limping into the game.
std::unique_ptr<View> create_viewer(GameContext& context, GameState& state,
                                    const std::filesystem::path& filename,
                                    ViewerType type) {
  if (filename.empty()) {
    return nullptr;
  }
  // fonts, sounds, pal and char are not actually filenames but just tell us
  // which built-in viewer to open. We'll deal with proper arguments maybe
  // in the future since it's just for debugging.
  if (filename == "fonts") {
    return std::make_unique<FontViewer>(context, state);
  }
  if (filename == "sounds") {
    return std::make_unique<SoundViewer>(context, state);
  }
  if (filename == "pal") {
    return std::make_unique<PaletteView>(context, state);
  }
  if (filename == "char") {
    return std::make_unique<CharacterViewer>(context, state);
  }
  if (type == ViewerType::Card) {
    return std::make_unique<CardViewer>(context, state, filename.string());
  }

  if (!std::filesystem::exists(filename)) {
    std::cerr << "File not found: " << filename << "\n";
    exit(1);
  }

  // If explicit type is provided, use it; otherwise detect from extension
  ViewerType effective_type = type;
  if (effective_type == ViewerType::Auto) {
    std::string ext = filename.extension().string();
    if (ext == ".vga") {
      effective_type = ViewerType::Vga;
    }
    else if (ext == ".bob") {
      effective_type = ViewerType::Bob;
    }
    else {
      effective_type = ViewerType::Vga;
      std::cerr << "Unknown file extension: " << ext << ", assuming VGA\n";
    }
  }

  try {
    switch (effective_type) {
    case ViewerType::Vga:
      Logger::info("Loading VGA file: {}", filename.string());
      return std::make_unique<VgaViewer>(context, state, filename.string());
    case ViewerType::Bob:
      Logger::info("Loading BOB file: {}", filename.string());
      return std::make_unique<BobViewer>(context, state, filename.string());
    case ViewerType::Auto:
    case ViewerType::Card:
      break; // resolved above
    }
  }
  catch (const std::exception& e) {
    Logger::warn("Failed to load viewer: {}", e.what());
    std::cerr << "ERROR: Failed to load file: " << filename << "\n";
    std::cerr << "Details: " << e.what() << "\n";
    exit(1);
  }
  catch (...) {
    Logger::warn("Unknown error loading viewer for: {}", filename.string());
    std::cerr << "ERROR: Unknown error loading file: " << filename << "\n";
    exit(1);
  }
  return nullptr;
}

} // namespace

ViewFactory::Result ViewFactory::create(GameContext& context, GameState& gs) {
  std::unique_ptr<View> view;

  // Default to game; menu screens below flip this. Applied to the cursor and
  // popups once after we know which screen we are building.
  UiMode ui_mode = UiMode::Game;
  std::visit(
      overloaded{

          [&](const ScreenTag& s) {
            using K = ScreenTag::Kind;
            if (is_menu_screen(s.kind)) {
              ui_mode = UiMode::Menu;
            }
            switch (s.kind) {
            case K::GameLocation:
              view = std::make_unique<GameLocationScreen>(context, gs);
              break;
            case K::Intro:
              view = std::make_unique<Intro>(context, gs);
              break;
            case K::MenuMain:
              view = std::make_unique<MenuMain>(context, gs);
              break;
            case K::MenuHighScores:
              view = std::make_unique<MenuHighScores>(context, gs);
              break;
            case K::MenuNewGame:
              view = std::make_unique<MenuNewGame>(context, gs);
              break;
            case K::MenuOptions:
              view = std::make_unique<MenuOptions>(context, gs);
              break;
            case K::MenuBindings:
              view = std::make_unique<MenuBindings>(context, gs);
              break;
            case K::MenuQuickstart:
              view = std::make_unique<MenuQuickstart>(context, gs);
              break;
            case K::CharacterSelection:
              view = std::make_unique<CharacterSelection>(context, gs);
              break;
            case K::DifficultySelection:
              view = std::make_unique<DifficultySelection>(context, gs);
              break;
            case K::DiskMenu:
              view = std::make_unique<DiskMenu>(context, gs);
              break;
            case K::MenuGameLoad:
              view = std::make_unique<MenuGameLoad>(context, gs);
              break;
            case K::MenuSave:
              view = std::make_unique<MenuSave>(context, gs);
              break;
            case K::MenuSaveAsGame:
              view = std::make_unique<MenuSaveAsGame>(context, gs);
              break;
            case K::MenuSaveAsMission:
              view = std::make_unique<MenuSaveAsMission>(context, gs);
              break;
            case K::MissionSelect:
              view = std::make_unique<MenuMissionSelect>(context, gs);
              break;
            case K::Advertising:
              view = std::make_unique<Advertising>(context, gs);
              break;
            case K::StaffView:
              view = std::make_unique<StaffView>(context, gs);
              break;
            case K::Newspaper:
              view = std::make_unique<Newspaper>(context, gs);
              break;
            }
          },

          // data-bearing screens
          [&](const MarketResearchState& mr) {
            view = std::make_unique<MarketResearch>(context, gs,
                                                    mr.district_index);
          },
          [&](const EuroMapState& em) {
            view = std::make_unique<EuroMap>(context, gs, em.mode, em.category);
          },
          [&](const RestaurantViewState& mm) {
            view = std::make_unique<RestaurantView>(context, gs, mm);
          },
          [&](const CityViewState& cm) {
            view = std::make_unique<CityView>(context, gs, cm.city_id);
          },
          [&](const PizzaJudgesState& pj) {
            view = std::make_unique<PizzaJudges>(
                context, gs, pj.scores, pj.on_complete, pj.judging_mode);
          },
          [&](const CategoryOverviewState& os) {
            view = std::make_unique<CategoryOverviewScreen>(
                context, gs, os.type, os.demographics);
          },
          [&](const IngredientCategoryViewState& is) {
            view = std::make_unique<IngredientCategoryView>(context, gs,
                                                            is.category);
          },
          [&](const DepotIngredientCategoryViewState& ds) {
            view = std::make_unique<DepotIngredientCategoryView>(context, gs,
                                                                 ds.category);
          },
          [&](const DepotMainState& dms) {
            view = std::make_unique<views::screens::depot::DepotMain>(context,
                                                                      gs, dms);
          },
          [&](const DepotExportState& des) {
            view = std::make_unique<DepotExportScreen>(
                context, gs, des.category, des.target_city_id,
                des.transport_cost);
          },
          [&](const DepotWeaponInterfaceState& dwis) {
            view =
                std::make_unique<views::screens::depot::DepotWeaponInterface>(
                    context, gs, dwis);
          },
          [&](const StockMainState& /*sms*/) {
            view =
                std::make_unique<views::screens::stock::StockMain>(context, gs);
          },
          [&](const ContractsViewState& /*cvs*/) {
            view = std::make_unique<Contracts>(context, gs);
          },
          [&](const IngredientSupplierListState& isls) {
            view = std::make_unique<IngredientSupplierList>(context, gs,
                                                            isls.category);
          },
          [&](const DepotIngredientSupplierListState& ds) {
            view = std::make_unique<DepotIngredientSupplierList>(
                context, gs, ds.category, ds.mode);
          },
          [&](const IngredientOrderState& ios) {
            view = std::make_unique<IngredientOrderScreen>(
                context, gs, ios.supplier_id, ios.category,
                ios.saved_ingredients);
          },
          [&](const DepotIngredientOrderState& ds) {
            view =
                std::make_unique<DepotIngredientOrderScreen>(context, gs, ds);
          },
          [&](const SupplierContractState& scs) {
            // Find the supplier from the city
            auto* city = gs.active_city();
            ActorType supplier_type =
                core::food::CategoryRegistry::supplier_type(scs.category);
            const FoodSupplier* supplier = nullptr;
            for (const auto* s : city->food_suppliers(supplier_type)) {
              if (s->id() == scs.supplier_id) {
                supplier = s;
                break;
              }
            }
            if (supplier) {
              auto conversation = std::make_unique<SupplierConversation>(
                  gs, supplier, scs.order, scs.total);
              auto* conv_ptr = conversation.get();
              // On success, pop IngredientOrderScreen too so we don't return
              // to it after the contract is signed.
              conversation->on_end_ = [&gs, conv_ptr]() {
                if (conv_ptr->contract_accepted()) {
                  IngredientOrderScreen::charge_exit(gs);
                  gs.ui().pop();
                }
              };
              Actor* actor_ptr = conversation->owned_actor();
              view = std::make_unique<ActorView<SupplierLine>>(
                  context, gs, actor_ptr, std::move(conversation));
            }
          },
          [&](const FurnitureListState& fs) {
            view = std::make_unique<FurnitureListView>(context, gs,
                                                       fs.supplier_id);
          },
          [&](const FloorListState& fs) {
            view = std::make_unique<FloorListView>(context, gs, fs.supplier_id);
          },
          [&](const PurchasingState& /*ps*/) {
            view = std::make_unique<PurchasingMain>(context, gs);
          },
          [&](const SupplierListState& ss) {
            view = std::make_unique<SupplierList>(context, gs, ss);
          },
          [&](const BankState& bs) {
            const auto& city = gs.active_city();
            auto* actor = city->actor_by_id(bs.id);
            auto conversation = std::make_unique<BankConversation>(
                gs, actor, city->bank_settings(actor->id()));
            view = std::make_unique<ActorView<BankLine>>(
                context, gs, actor, std::move(conversation));
          },
          [&](const PoliceState& ps) {
            auto* actor = gs.active_city()->actor_by_id(ps.id);
            auto conversation =
                std::make_unique<PoliceConversation>(gs, actor, ps.reason);
            auto police_view = std::make_unique<ActorView<PoliceLine>>(
                context, gs, actor, std::move(conversation));
            police_view->set_on_complete(ps.on_complete);
            view = std::move(police_view);
          },
          [&](const MafiaState& ms) {
            auto* actor = gs.active_city()->actor_by_id(ms.id);
            auto mode = ms.mode.value_or(MafiaMode::PlayerCall);
            auto conversation = std::make_unique<MafiaConversation>(
                gs, actor, mode, ms.restaurant_id, ms.first_contact);
            view = std::make_unique<ActorView<MafiaLine>>(
                context, gs, actor, std::move(conversation));
          },
          [&](const MayorState& mys) {
            auto* city = gs.active_city();
            auto* actor = city->actor_by_id(mys.id);
            std::optional<MayorConversation::InspectionOutcome> opt_outcome;
            if (mys.is_first_inspection.has_value()) {
              opt_outcome = MayorConversation::InspectionOutcome{
                  .star_gained = mys.inspection_star_gained,
                  .star_lost = mys.inspection_star_lost,
                  .is_first = *mys.is_first_inspection,
              };
            }
            std::string restaurant_name;
            if (mys.restaurant_id.has_value()) {
              if (auto* r = gs.restaurant(*mys.restaurant_id)) {
                restaurant_name = r->name();
              }
            }
            auto conversation = std::make_unique<MayorConversation>(
                gs, actor, city->mayor_office(), mys.type, opt_outcome,
                mys.award_winner_player_id, restaurant_name);
            if (mys.on_complete) {
              conversation->on_end_ = mys.on_complete;
            }
            view = std::make_unique<ActorView<MayorLine>>(
                context, gs, actor, std::move(conversation));
          },
          [&](const TerminalState& ts) {
            view = std::make_unique<Terminal>(context, gs, ts);
          },
          [&](const ActorTerminalState& at) {
            view = ActorViewFactory::create(context, gs, at);
          },

          [&](const GameOverState& gos) {
            ui_mode = UiMode::Menu;
            view = std::make_unique<GameOver>(context, gs, gos);
          },
          [&](const MaestroMessageState& mms) {
            view = std::make_unique<MaestroMessage>(context, gs, mms);
          },
          [&](const ViewerState& v) {
            ui_mode = UiMode::Menu;
            // Use the viewer type stored in the ViewerState
            view = create_viewer(context, gs, v.filename, v.type);
          },
          [&](const OptionsMenuState& oms) {
            ui_mode = UiMode::Menu;
            view = std::make_unique<MenuOptions>(context, gs, oms.screen,
                                                 oms.focus_advanced_toggle);
          },
          [&](const RestaurantMenuState& rms) {
            view = std::make_unique<RestaurantMenu>(context, gs, rms.id);
          },
          [&](const RestaurantStatsState& rss) {
            std::optional<RestaurantStatsScreen> screen = std::nullopt;
            if (rss.screen_index.has_value()) {
              screen =
                  static_cast<RestaurantStatsScreen>(rss.screen_index.value());
            }
            view = std::make_unique<RestaurantStats>(
                context, gs, rss.id, screen, rss.automatic_mode);
          },
          [&](const WeeksTakingsState& wts) {
            view = std::make_unique<WeeksTakings>(
                context, gs, wts.id, wts.end_of_week_mode, wts.on_done);
          },
          [&](const GlobalStatsState& gss) {
            std::optional<GlobalStatsScreen> screen;
            if (gss.screen_index) {
              screen = static_cast<GlobalStatsScreen>(*gss.screen_index);
            }
            view = std::make_unique<GlobalStats>(context, gs, screen);
          },
          [&](const StatsRestaurantsState& srs) {
            view =
                std::make_unique<StatsRestaurants>(context, gs, srs.player_id);
          },
          [&](const AnimatedStatsState& ass) {
            view = std::make_unique<AnimatedStats>(context, gs, ass.id,
                                                   ass.screen);
          },
          [&](const PizzaDesignState& pms) {
            view = std::make_unique<PizzaDesign>(context, gs, pms);
          },
          [&](const PlayerTurnCardState& ptcs) {
            view = std::make_unique<PlayerTurnCard>(context, gs, ptcs);
          },
          [&](const FullscreenStatusState& fss) {
            view = std::make_unique<FullscreenStatus>(
                context, gs, fss.message_id, fss.on_dismiss);
          },
          [&](const WinSequenceState& wss) {
            view = std::make_unique<WinSequenceView>(
                context, gs, wss.line_index, wss.player_id);
          },
          [&](const MafiaMissionViewState& mmvs) {
            view = std::make_unique<MafiaMissionView>(context, gs, mmvs.line);
          },
          [&](const PlayerProfileState& pps) {
            view = std::make_unique<PlayerProfile>(context, gs, pps.player_id);
          },
          // no screen
          [&](const std::monostate&) { /* leave view == nullptr */ }

      },
      gs.ui().top());

  // Wrap in PaneledView if necessary
  if (view && !dynamic_cast<PaneledView*>(view.get())) {
    if (dynamic_cast<MainPanelView*>(view.get())) {
      view = std::make_unique<PaneledView>(context, gs, std::move(view));
    }
  }

  return Result{.view = std::move(view), .ui_mode = ui_mode};
}
