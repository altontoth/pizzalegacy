# Modernizations & fixes

This document lists intentional differences from the original game. Some are
life quality improvements gated by an option, others are bug fixes for original
bugs (gated by a separate option), a couple are advanced options, and then
there are some that are just always in effect.

## Always in effect

### Game options

* In the load game you can now see information related to the savegame, like the
  time the save was made, in game date, player names and currently active player
  and city.
* You can now delete save games from this menu.
* The file list also shows the last modified date on the savegame file.
* The file box can be scrolled with the mouse scroll wheel.
* Menus can be navigated with the keyboard.
* Game options are split into sections, with new options added for engine and
  gameplay features (e.g. FPS, vsync, auto save, etc.).

### Shift-Tab goes to previous restaurant

* Like in the original you can switch between restaurants by pressing the "Tab"
  key from the restaurant view; in Pizza Legacy you can also press Shift+Tab to
  go to the previous restaurant.

### City map scrolling

* When scrolling on the city map using the arrow keys, you can now press two
  keys at once, e.g. left and up to move diagonally.

* Pressing Shift on the keyboard now speeds up scrolling.

### Popup keyboard interaction

* You can respond to confirmation popups with "Y" or "Enter" to confirm and "N"
  or "Escape" to decline.
* Alert popups can be dismissed with "Enter" or "Escape".
* The number input popup responds to the Up and Down arrow keys to change the
  value and then Enter to accept.

### Ingredient ordering speed

* In the original you could speed up the ingredient counts by holding the other
  mouse button, e.g. when holding the Right Mouse Button to increase the
  number, if you then pressed Left Mouse Button as well it would speed up the
  count (and vice-versa to decrease). This still works, but instead of holding
  both mouse buttons you can now also press the Shift key on the keyboard, to
  make this easier for user with trackpads.

* The speed up also works when ordering ingredients from a restaurant, not just
  in the warehouse.

### Number input manual input

* In the number input popup (used for example to set staff wages) you can also
  double click on the value and type a number directly, instead of only using
  the arrows to change the value.

### Pause game

* In the original you can pause the game by going to the Disk Menu, but in
  Pizza Legacy you can also pause by pressing `Control+P`.

### RMB/LMB alignment for ingredient ordering

* When increasing your ingredient order from a restaurant you use Left Mouse
  Button (LMB) to increase the count and Right Mouse Button (RMB) to decrease
  the count. In the depot interface in the original that is switched around, you
  use RMB to increase and LMB to decrease. In Pizza Legacy I've normalized this
  to always use LMB for increasing and RMB for decreasing.

### Faster ordering in depots

* While increasing or decreasing the count in the depot ingredienet interface,
  you can press Shift to add 5 per tick, Control to add 50 per tick and
  Control+Shift to add 100 per tick, for later game when you have a lot of
  restaurants and need to order a lot.

### Mouse scroll wheel

* In various screens you can now use the mouse wheel, for example to change the
  ingredient and furniture order count, switch between candidate cards in the
  staff screen and when selecting characters when creating a new game.

### Busy cursor and activity captions

* When the game is busy and not accepting input (ending a turn, autosaving,
  loading a save, initializing audio) the cursor changes to a padlock and a
  short caption in the bottom left corner says what is happening ("Ending
  turn...", "Saving...", "Loading..."). The original didn't need this: in 1994
  your HDD or floppy LED (and drive noise) told you the machine was working.
  On silent modern hardware a stalled frame reads as a hang, so Pizza Legacy
  provides that feedback on screen instead.

### Animated stats titles always visible

* In the original the 'restaurant popularity' and 'quality of pizza' titles on
  these respective animated stats screen are only visible for the first frame,
  any animation afterwards lacks these titles. In Pizza Legacy they are always
  visible.

## Life quality improvements option

We have a new option to optionally make some more improvements:

### Larger highlight dot on city map

When the city map is fully zoomed out, the yellow dot highlighting whatever you
selected to be highlighted will be double the size compared to the original,
since often it can be hard to see otherwise.

### Shortage status message include restaurant name
Messages like _Too few ovens to satisfy present demand!_ now include the restaurant name.

### Don't ask to sell furniture
In the original when you sell furniture it asks if you're sure you want to sell
it for half the price you bought it for. But if you throw away the item you
don't get any questions it just does it immediately. With the QoL setting on we
just sell it immediately.

### Market Research bar charts are more fine grained
Original started at $10,000, with this setting on it starts at $1,000 so you
can see more detail early on in the game.

### Tooltips
Add tooltips to various screens for more clarity on what things mean.

### Capacity bars rescale with demand
In the original the capacity screen's bars stop growing once demand passes a
fixed limit, so any shortages could be hidden if demand and supply were high
enough. Now the scale widens to fit demand, so you can always see if one of the
things in that screen is a bottleneck.

### Euromap destination selection
When selecting destinations in the original, after clicking on the city you
have to click the box with the text that tells you what the costs are; with the
QoL setting on you can also just click the city again.

### Character selection menu
With the option on it shows an explanation on the "Increase/decrease number of
human players" arrow buttons.

### Ingredient order remembered when contract fails
If you try to take a contract with a food supplier but for some reason are
unable to (order too small, or you tried to negotiate but failed) then now your
order is remembered so you can make adjustments to the order and try again
without having to make the whole selection again.

### Map scrolling
You can now scroll the map by using the right mouse button and dragging. This
works on both the city view and the restaurant view.

### Zooming using mouse wheel
In the city view you can use the mouse wheel to change zoom level.

## Bug fixes

If the *Fix original bugs* option is on we fix the following issues that I assume are bugs:

### Chemical contamination
This never resets, so any chemical attack permanently damages the restaurant
(and it stacks). The fix decays the chemical damage by one every day.

### "You have achieved 10% market share" fires at 5%
When you reach 5% market share it would show the “You have 10% market share”.
The fix delays this until 10% market share is reached.

### Game mode in Player profile screen incorrect
In the player profile screen it shows game mode "Free mode” even when doing a
Quick start. Not sure if this is a bug or a choice, but if bug fixes are
enabled we show "Quick start" here in that case.

### Trade embargoes were not enforced
Trade embargoes were announced in the newspaper but did not actually prevent
trading. If bug fixes are enabled it now prevents you from trading with
embargoed cities.

### Newspaper event to ingredient mapping
The original had various bugs in the newspaper event system, e.g. when telling
you pineapple was super out it would affect raspberries instead.

### Contract penalty halved despite refusal
When a skill check failed for complaining about the penalty clause with a
supplier contract, the original would halve it despite saying not seeing a
reason to do so. Now the penalty is only reduced if the checks actually all
succeed.

### Mission date mismatch
The original mission files were created for the German release, which starts in
1994. The English release starts in 1995, making it impossible to score 100% on
the missions because they are supposed to happen before the game even begins.

With the fix enabled, mission dates are shifted one year forward to match the
English release timeline.

### City economy sometimes stuck on one trend for five years
Each city's economy drifts towards a goal, and picks a new goal when a countdown
of up to 39 weeks runs out. The countdown in the original is a byte and can be
rolled as zero, in which case the very next decrement wraps it around to 255 and
the city would keep the same trend for 256 weeks instead.

With fixes enabled the countdown is always at least one week, so it never
wraps.

### Failed haggle with a realtor lowers a tenant’s price

When a realtor refuses your attempt to haggle (after you say “the price is too
high”), the price of the property goes up by 10% permanently. In the original
it does that by storing the price it just quoted you (original + 10%). But, if
you are already the tenant of the shop it gives you a 70% discount to start
with. This means that after a refusal the shop is quoted at 77% of its value.
And it stores this value, so if you try multiple times to negotiate, the 70%
discount is applied again and again. The same works in reverse when the realtor
is trying to rip you off: the doubled price gets stored, so it is inflated
again on the next call.

With the fix enabled, a refusal still permanently raises the property’s stored
price by 10%, but the 70% tenant discount is applied only when calculating the
price shown to the tenant; it is no longer included in the price that gets
stored.

### Paid-off loans stay on the balance sheet
The monthly loan payment is rounded down to whole dollars, so a loan is paid
off a few dollars short (a $5000 loan over 12 months leaves $8). That remainder
is never charged and stays under loans on the balance sheet for good.

With the fix enabled, the last monthly payment or an early repayment also pays
the remainder.

## Advanced options

Some of what I think were bugs have a big impact on the gameplay; given that
the game was balanced with those bugs in it, the game might suddenly be too
difficult with them fixed. To allow play testing with the new behavior these
options, along with a few timing and display settings, are hidden on the
options screens until you tick "Show advanced options" in the bottom left
corner. That checkbox is not saved, so it is off again every time the game
starts. The balance options and the player colors are on the Gameplay screen,
the timing options on Controls. settings.conf carries a commented list of every
option and its default as well.

### mafia_shakedowns_target_all_restaurants
The original iterates over all restaurants but never assigns the result to
anything, so it actually only checks the first restaurant. That means that once
you have protection for the first restaurant you don't get mafia shakedown
requests for the other restaurants. With this option on it doesn't just target
the first restaurant but picks a random restaurant so all your restaurants have
a chance to be the target of a shakedown.

### demand_fulfillment_popularity
Popularity is supposed to take a hit when a restaurant couldn’t fill its orders
the day before, up to -12. The DOS version clears that number right before it
reads it, so every restaurant gets the full -12 penalty. This option (on by
default) fixes it, increasing popularity and thus the number of customers a
bit.

The original popularity thermometer also doesn’t animate reductions in
popularity; ours does, regardless of this option.

### show_player_color_in_dropdown
The Player and Range dropdowns on the city map show each player's control
map color in front of their name. On by default.

### win_condition
This can take one of three options; the German release requires >= 5% market
share in all ten cities to win the game (`all_cities`), the English version
only checks Paris (`paris`), which looks like it was meant to be any single
city (`any_city`).
