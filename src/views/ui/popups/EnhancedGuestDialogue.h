/*
 * Additional original dialogue for Pizza Legacy's enhanced mode.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <string_view>

namespace enhanced_guest_dialogue {
struct Group {
  int first, last;
  std::string_view casual[2];
  std::string_view formal[2];
};
// Indexed by the original GTALK line selected by the simulation. Only
// equivalent sentiments are substituted; original assets remain untouched.
inline constexpr Group groups[] = {
    {1, 2, {"That pizza is a knockout!", "Now THAT is a pizza!"}, {"An outstanding flavour!", "I shall certainly order this again."}},
    {3, 4, {"Hey, that's really tasty!", "You've got something good going here."}, {"A very enjoyable pizza.", "The flavour is quite impressive."}},
    {5, 6, {"Not bad for a quick bite.", "That'll do me just fine."}, {"A perfectly satisfactory meal.", "Quite pleasant, on the whole."}},
    {7, 8, {"I've had better at the bus station.", "This needs a bit more kick."}, {"The flavour leaves something to be desired.", "I had hoped for rather more."}},
    {9, 10, {"My taste buds want a refund!", "Who dared you to make this?"}, {"I cannot recommend this to anyone.", "A most unfortunate culinary experiment."}},
    {11, 11, {"Pizza AND change for the arcade!", "At that price, I'll have seconds!"}, {"Remarkably good value.", "A bargain by anyone's standards."}},
    {12, 12, {"Yeah, that's a fair deal.", "My wallet can live with that."}, {"A reasonable price for a meal.", "Quite fairly priced."}},
    {13, 13, {"Bit steep for a pizza, isn't it?", "My pocket money won't stretch much further."}, {"Rather dear, I would say.", "The price is somewhat ambitious."}},
    {14, 14, {"What am I paying for, the oven?", "At that price I want the recipe too!"}, {"An entirely unreasonable price.", "I shall think twice before returning."}},
    {15, 15, {"Better deal than the place down the road!", "Now that's how you beat the competition."}, {"Your competitors could learn from these prices.", "Excellent value compared with other establishments."}},
    {16, 16, {"Pretty fair for this part of town.", "Can't complain about that price around here."}, {"Reasonable for this neighbourhood.", "The local prices seem comparable."}},
    {17, 18, {"My wallet's just walked out the door.", "Next time I'm eating somewhere cheaper."}, {"I shall dine elsewhere at these prices.", "The cost is quite out of proportion."}},
    {19, 20, {"This looks like a million bucks!", "Someone in that kitchen knows their stuff!"}, {"Beautifully presented and expertly prepared.", "A credit to your kitchen."}},
    {21, 22, {"Nice work on this one!", "That's a pretty professional pizza."}, {"Very nicely prepared.", "The workmanship is commendable."}},
    {23, 24, {"Looks alright to me.", "Nothing wrong with that."}, {"The preparation is satisfactory.", "A competently made pizza."}},
    {25, 26, {"Nothing to write home about.", "Looks like a pretty ordinary pizza."}, {"Unremarkable, but adequate.", "Rather ordinary in its preparation."}},
    {27, 28, {"I could do better with my eyes shut!", "Did the recipe fall behind the oven?"}, {"The preparation needs considerable attention.", "I expected a more carefully made pizza."}},
    {29, 30, {"Was the chef having a bad day?", "This pizza needs emergency repairs!"}, {"This is a most disappointing effort.", "I would question the standards in your kitchen."}},
    {31, 32, {"This place looks seriously classy!", "Nice furniture! Where'd you get it?"}, {"The furnishings are quite splendid.", "A beautifully appointed restaurant."}},
    {33, 34, {"Cool decor! I like it here.", "You've got a good look going."}, {"The decor is very agreeable.", "A most comfortable setting."}},
    {35, 35, {"Yeah, I could get used to this.", "The place is growing on me."}, {"The surroundings are acceptable.", "One could become accustomed to the decor."}},
    {36, 37, {"Did you buy this stuff at a yard sale?", "This place needs a serious makeover."}, {"The furnishings are not to my taste.", "A refurbishment would be most welcome."}},
    {38, 38, {"Great vibe in here!", "This is my kind of hangout."}, {"A very pleasant atmosphere.", "The ambience is quite delightful."}},
    {44, 44, {"Crank up the tunes!", "That jukebox rocks!"}, {"An excellent choice of music.", "The musical selection is most agreeable."}},
    {45, 45, {"Who put that record on?!", "My ears need a break!"}, {"The music is rather disagreeable.", "I would prefer a different musical selection."}},
    {56, 56, {"Not now, mate!", "I'm busy, alright?"}, {"I would prefer not to be disturbed.", "Perhaps another time."}},
    {57, 57, {"You again? Let me eat!", "Didn't we just talk?"}, {"Is there something else you require?", "I believe we have already spoken."}},
};
inline std::string_view variant(int line, bool formal, int index) {
  for (const auto& group : groups) {
    if (line >= group.first && line <= group.last) {
      return formal ? group.formal[index] : group.casual[index];
    }
  }
  return {};
}
} // namespace enhanced_guest_dialogue
