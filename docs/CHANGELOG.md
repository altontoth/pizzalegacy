1.0.0-beta.1 (2026-07-11)

First feature-complete release: all gameplay systems from the original game
are now implemented.

* Add final gameplay systems
  * Mission mode
  * Newspaper advertising mini-game
  * Staff training: course selection and completion effects on stats
  * Restaurant attacks: enter a competitor's restaurant outside business hours
    with a weapon and cause damage; human defenders can fight back with
    WASD/Left Ctrl or gamepad
  * Walking guests: guests now visibly enter and leave the restaurant, and the
    waiter brings them their pizza
  * Cookbook reminder when recreating a standard recipe scores below 10%
    imitation, like in the original

* UI and options
  * Keyboard navigation of menus: arrows to move, tab/shift+tab to skip over
    widgets with their own items
  * Options screen split into categories (audio, graphics, controls, gameplay)
  * Volume sliders in the audio menu
  * FPS setting (30/60/120) and VSync setting
  * Software cursor (default): the cursor is drawn by the game renderer so it
    can no longer outpace the game, fixing lagging placement of ingredients in
    Pizza Design during fast mouse movement; hardware cursor remains available
    as an option
  * Gamepad support and key bindings on modifier keys

* Fixes
  * Job market: fired staff can no longer be immediately rehired
  * First mafia mission no longer always fails, and a wrongly scoped mafia
    milestone was corrected
  * Restaurant/depot closure now has its missing social score impact
  * Conversations no longer push time past the end of day
  * Standard pizza judging bug fixed
  * Capacity screen now uses today's forecast like the production screen,
    matching the original
  * Cursor aspect ratio in fullscreen on Windows (thanks to Reddit user
    plonspfetew for the report)

* Internals
  * Migrated from SDL2 to SDL3

0.1.0 (2026-05-25)

* Add missing screens
  * Daily profits
  * Cash flow/liquidity
  * Restaurant sales curve
  * Restaurant sales figures
  * Customer tastes
  * Property overview

* Add interactions
  * Mafia with missions
  * Mayor with awards and restaurant stars
  * Police with arrests
  * Insurance with buying and claiming
  * Bank investments and money transfers
  * Staff random events (lateness, pregnancy, theft, mistakes)
  * Pizza competitions (end of month/end of year)

* Add game systems
  * Fullscreen status messages and win/lose scenarios
  * Supplier delivery contracts
  * Job market: character availability scaled by difficulty; expand to max 200
    characters via the original's character doubling system
  * Manager role: handles staff events, restocks ingredients, adjusts menu
    prices autonomously
  * Sabotage: joke articles (smelly cheese, laxatives, rats, stink bombs)
  * Autosave

* Various UI changes
  * Load game screen: list sorting, show last modified date in list
  * Mouse wheel support expanded: city map zoom, staff candidate cards, ingredient/furniture
    order counts, furniture category switching, number input popup
  * Right-click map scrolling in city and restaurant views (grab-and-drag and inverse)
  * Pizza menu prices now editable after adding to menu
  * Status messages behave closer to original

* Various game changes
  * Operations now cost time (placing furniture, calling people)
  * Fix bugs (from original) in ingredient related newspaper events
  * Food supplier sort order corrected to match original; cut suppliers removed from list

* Internals
  * New, smaller, savegame format (not compatible with v0.0.1 saves)
  * Audio engine optimizations and initialization moved to intro screen
  * Rewrote end-of-turn system completely for better code clarity
  * Steam and GOG app folder auto-detection when configuring the PT.EXE path


0.0.1 (2026-03-07)

* Initial release
