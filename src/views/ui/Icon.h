/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

constexpr int ICONS_PER_ROW = 16;
constexpr int ICONS_PER_COLUMN = 6;
constexpr int ICON_WIDTH = 32;
constexpr int ICON_HEIGHT = 26;

// Icons as they appear in gfx/icons.vga

enum class Icon {
  ARROW_BACK,
  ARROW_UP,
  ARROW_DOWN,
  GRAPH,            // ECONOMY & GLOBAL STATISTICS
  CONTRACTS,        // contracts
  EXCLAMATION_MARK, // depot priorities
  AIRPLANE,         // map
  CHARACTER,        // Character mode
  DISK,             // Disk options
  M,                // unknown
  MEGAPHONE,        // Advertising
  TRUCK,            // Move furniture elsewhere
  GLOBE,            // Exports
  STOCK,
  PARCHMENT, // Recipe list
  FURNISHING,
  CHARACTER2, // Customer stats
  SAFE,       // unknown
  THUMBS_UP,
  THUMBS_DOWN,
  BUY,              // anything else?
  SELL,             // and transfer
  QUESTION_MARK,    // market research
  HOUSE,            // property and property info
  LEFT_RIGHT_ARROW, // unknown
  SHOPPING_CART,    // buy/hire
  SHAKING_HANDS,    // close contract
  VEGETABLE,        // vegetable (carrot)
  MEAT,             // meat (chicken drum)
  FRUIT,            // fruit (apple)
  CHEESE,           // cheese
  BOTTLE,           // unknown, maybe from when they had drinks in the game
                    // before dropping it?
  BOMB,             // access weapons in depot
  CHAIR,            // furniture
  OVEN,             // oven
  FOOT,             // flooring
  PLANT,            // extras
  KEY,              // Close / open the store
  EXAMINE,          // Pizza info, view ads
  PIZZA,            // View pizza
  NEWSPAPER,
  MONEY_BAG, // Weeks takings & loans & sell it
  PADLOCK,   // insurance
  HEARTS,    // unknown
  LEFT_ARROW,
  RIGHT_ARROW,
  MENU,   // menu
  MEDALS, // awards
  ONE,
  TWO,
  THREE,
  FOUR,
  FIVE,
  SIX,
  SEVEN,
  EIGHT,
  NINE,
  CALCULATOR, // pizza sales
  WEIRD_A,    // unknown
  ALARM_CLOCK,
  CHEFS_HAT,               // chefs
  TIE,                     // managers
  COMPUTER,                // terminal
  BOW_TIE,                 // waiters
  CUP,                     // unknown
  UNKNOWN_GUY,             // unknown
  BOOT,                    // Fire someone
  FORM,                    // training
  PODIUM,                  // ratings, pizza charts
  BALANCE,                 // balance
  INFO,                    // area info
  ARROW_UP_DOWN,           // trends
  CUSTOMER_TASTE,          // unknown (no idea what this is supposed to look
                           // like...)
  CUBE,                    // market share
  BAR_CHART,               // competitors
  ROTATING_ARROWS,         // production
  P,                       // reputation
  Q,                       // pizza quality
  CHARACTER_QUESTION_MARK, // unkonwn
  F,                       // Company
  TARGET_CV,               // target cv
  FANCY_R,                 // Position (or is it a death with a scyth?)
  DAILY_BALANCE,  // unknown what this represents (FR something something)
  X,              // unknown (unused?)
  SUNSET,         // end of day
  TELEPHONE,      // dial
  ABC_REPORT,     // unknown
  WRITTEN_REPORT, // CV
  RESTAURANT,     // restaurants
  MONEY_TAP,      // cash flow
  POT,            // bake pizza
  CAPACITY,       // capacity
};
