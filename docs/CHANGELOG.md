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
