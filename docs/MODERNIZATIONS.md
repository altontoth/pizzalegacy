# Modernizations & fixes

This document lists intentional differences from the original game. Some are
life quality improvements gated by an option, others are bug fixes for original
bugs (gated by a seperate option) and then there are some that are just always
in effect.

## Always in effect

### Load Game options

* In the load game you can now see information related to the savegame, like the
  time the save was made, in game date, player names and currently active player
  and city.
* You can now delete save games from this menu.
* The file list also shows the last modified date on the savegame file.
* The file box can be scrolled with the mouse scroll wheel.

### Shift-Tab goes to previous restaurant

* Like in the original you can switch between restaurants by pressing the "Tab"
  key from the restaurant view; in Pizza Legacy you can also press Shift+Tab to
  go to the previous restaurant.

### Game options

* Instead of separate "Save" and "Exit" options there is now a "Save and exit"
  option; I think the options are simple enough that requiring two clicks to
  save the changes is not worth it. It was either this or a "Are you sure you
  want to exit without saving" popup, which would also be annoying.

### City map scrolling

* When scrolling on the city map using the arrow keys, you can now press two
  keys at once, e.g. left and up to move diagonally.

* Pressing Shift on the keyboard now speeds up scrolling.

### Confirmation popup keyboard interaction

* You can respond to confirmation popups with "Y" or "Enter" to confirm and "N"
  to decline. Did this specifically to make it easier to quit the game during
  debugging since introducing the confirmation popup there.

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

### Larger highlight dot on city map

* When the city map is fully zoomed out, the yellow dot highlighting whatever
  you selected to be highlighted will be double the size compared to the
  original, since often it can be hard too see otherwise.

### RMB/LMB alignment for ingredient ordering

* When increasing your ingredient order from a restaurant you use Left Mouse
  Button (LMB) to increase the count and Right Mouse Button (RMB) to dcrease
  the count. In the depot interface in the original that is switch around, you
  use RMB to increase and LMB to decrease. In Pizza Legacy I've normalized this
  to always use LMB for increasing and RMB for decreasing.

### Mouse scroll wheel

* In various screens you can now use the mouse wheel, for example to change the
  ingredient and furniture order count, switch between candidate cards in the
  staff screen and when selecting characters when creating a new game.

## Life quality improvements option

We have a new option to optionally make some more improvements:

### Pizza demand
We guarantee a small random minimum demand for any pizza on the menu. The
original's aggressive demographic scoring causes most pizzas to get 0 demand
for most demographics, making most of the 14 standard pizzas pointless to have
on the menu.

### Shortage status message include restaurant name
Messages like _Too few ovens to satisfy present demand!_ now include the restaurant name.

### Don't ask to sell furniture
In the original when you sell furniture it asks if you're sure you want to sell
it for half the price you bought it for. But if you throw away the item you
don't get any questions it just does it immediately. With the QoL setting on we
just sell it immediately.

### Market Research bar charts are more fine grained
Original started at $10,000, with this setting on it starts at $1,000 so you
can see more detail early on in the game. It also shows a tooltip when hovering
over the player icon that shows the exact amount.

### Euromap destination selection
When selecting destinations in the original, after clicking on the city you
have to click the box with the text that tells you what the costs are; with the
QoL setting on you can also just click the city again.

### Character selection menu
With the option on it shows an explanation on the "Increase/decrease number of
human players" arrow buttons.

## Ingredient order remembered when contract fails
If you try to take a contract with a food supplier but for some reason are
unable to (order too small, or you tried to negotiate but failed) then now your
order is remembered so you can make adjustments to the order and try again
without having to make the whole selection again.

## Map scrolling
You can now scroll the map by using the right mouse button and dragging. This
works on both the city view and the restaurant view.

## Zooming using mouse wheel
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

### Investments repay sooner
In the original when you make a two month investment, you get interested payed
at the end of month one, then again at the end of month two and then you get
your principal back on the end of month three. That's weird, with bug fixes
active you get the interest plus the principal back at the end of month two.

### Newspaper event to ingredient mapping
The original had various bugs in the newspaper event system, e.g. when telling
you pineapple was super out it would affect raspberries instead.

### Contract penalty halfed despite refusal
When a skill check failed for complaining about the penalty clause with a
supplier contract, the original would half it despite saying not seeing a
reason to do so. Now the penalty is only reduced if the checks actually all
succeed.

### Win condition fixed
In the original Pizza Tycoon there was a bug (at least I think it
wasn't on purpose?) that you had to have >= 5% market share in just Paris
instead of all 10 cities.
