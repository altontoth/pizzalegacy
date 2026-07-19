# Debug Console Command Reference

This document describes the runtime debug console commands registered in the game.

Source of truth:
- `src/core/debug/DebugCommands.cpp`
- `src/core/debug/commands/*.cpp`

## Opening the console

- Press Backtick (`) to toggle the console.
- Press Enter to submit input.

## Syntax conventions

- Arguments in `<angle_brackets>` are required.
- Arguments in `[square_brackets]` are optional.
- For IDs that support defaults, `@` means "active" (active player/city/restaurant).
- Many commands default to active entities when ID args are omitted.

## Top-level commands

- `help`
- `player` (alias: `p`)
- `restaurant` (aliases: `r`, `rest`)
- `depot` (aliases: `d`, `dep`)
- `game`
- `nav` (alias: `ns`)
- `debug`
- `dump` (top-level convenience alias of `debug dump`)
- `test` (top-level convenience alias of `debug test`)
- `city`
- `news`
- `mafia`
- `police`
- `mayor`
- `staff` (alias: `st`)
- `armsdealer`

---

## player

- `player info [player_id]`
- `player money set <amount> [player_id]`
- `player money add <amount> [player_id]`
- `player money sub <amount> [player_id]`
  - aliases for `sub`: `subtract`, `remove`
- `player inventory set <item_id> <amount> [player_id]`
- `player inventory clear [player_id]`
  - alias for `inventory`: `inv`
- `player recipes load [player_id] [index]`
  - `player recipes load` alone will load all of the canonical recipes

## restaurant

- `restaurant info [player_id] [restaurant_id]`
- `restaurant clear [player_id] [restaurant_id]`
- `restaurant furniture list [player_id] [restaurant_id]`
- `restaurant stock add <ingredient_id> [quantity] [quality] [player_id] [restaurant_id]`
- `restaurant stock fill <quantity_kg> <quality> [player_id] [restaurant_id]`
- `restaurant stock clear [player_id] [restaurant_id]`
- `restaurant create <city_id> <player_id> <size> <layout_id>`
  - `size`: `big` or `small`
- `restaurant qs <city_id> <player_id>`
  - quick-start helper that creates and boots a functional restaurant setup
- `restaurant inspection-timer <days> [player_id] [restaurant_id]`

## depot

- `depot give [city_id] [player_id]`
- `depot info [city_id] [player_id]`
- `depot stock add <ingredient_id> [quantity] [quality] [city_id] [player_id]`
- `depot stock fill [quantity] [quality] [city_id] [player_id]`
- `depot stock clear [city_id] [player_id]`
- `depot stock steal [city_id] [player_id]`

## game

- `game save <filename>`
- `game load <filename>`
- `game exit`
  - alias: `game quit`
- `game date`
- `game date set <dd-mm-yyyy>`
- `game time`
- `game time set <hh:mm>`
- `game competition monthly`
- `game end <cause>`
  - causes: `debts`, `suppliers`, `killed`, `tycoon`
- `game levent <id>`
  - valid range: `1-139`
- `game mission`
- `game mission write <name>`

## nav

General navigation helpers.

Simple screens:
- `nav intro` (alias: `is`)
- `nav main` (alias: `mm`)
- `nav ng`
- `nav op`
- `nav disk` (alias: `dm`)
- `nav advertising` (alias: `ad`)

Views with args:
- `nav cityview [city_id]` (aliases: `city`, `cv`)
- `nav restaurant [restaurant_id]` (aliases: `rest`, `rv`)

Pizza screens:
- `nav pizza design`
- `nav pizza recipes`

Other screens:
- `nav euromap` (alias: `em`)
- `nav terminal` (alias: `tm`)
- `nav purchasing` (alias: `pm`)
- `nav suppliers` (alias: `sl`)
- `nav stock` (alias: `st`)

## debug

- `debug test`
- `debug dump player [player_id]`
- `debug dump players`
- `debug dump ledger [player_id]`
- `debug dump state`
- `debug dump layers`
- `debug dump inventory [player_id]`
- `debug dump input <bindings|state>`

Top-level convenience equivalents:
- `test` (same as `debug test`)
- `dump ...` (same subcommands as `debug dump ...`)

## city

- `city chart show`
- `city chart debug [player_id]`
- `city chart rebuild`
  - alias for `chart`: `charts`

## news

Status:
- `news status`

Schedule:
- `news schedule list [all]`
- `news schedule set <slot> <event_id> <days>`
- `news schedule clear [slot]`
- `news schedule fire <event_id>`
- `news schedule generate`
  - alias: `news schedule gen`

Article:
- `news article show`
- `news article set <article_id> [cooldown]`
- `news article ingredient <slot> <polarity> <category>`
- `news article clear`
- `news article read`

Trends:
- `news trends list [all]`
- `news trends get <slot>`
- `news trends set <slot> <initial> <target> <duration>`
  - duration: `none`, `linear`, `exponential`, `countdown`
- `news trends clear [slot]`
- `news trends tick [n]`

## mafia

- `mafia shakedown [restaurant_id]`
- `mafia loan-due`
- `mafia mission-overdue`
- `mafia status`
- `mafia set-bust <true|false>`
- `mafia set-pickup <tile_index> [hour] [minute]`
- `mafia set-dropoff <tile_index> [hour] [minute]`
- `mafia set-deadline <days>`

## police

- `police arrest <crime_code>`
  - valid range in current implementation: `1-7`
- `police visit`
- `police status`

## mayor

- `mayor inspect [restaurant_id] [first]`
- `mayor award <type> [winner_player_id]`
  - types: `beauty`, `pizza`, `investor`, `imitator`, `bowtie`, `trade`
- `mayor favor <amount> [player_id]`
- `mayor status`

## staff

- `staff event <type> [player_id] [restaurant_id] [slot]`
  - types: `late`, `sick`, `maternity`, `quit`, `raise`, `theft`, `destroycheesestock`, `blowupoven`, `spillwine`, `messeduporders`, `forgottoopen`
- `staff set_motivation <value> [player_id] [restaurant_id] [slot]`
- `staff set_days_away <days> [player_id] [restaurant_id] [slot]`
- `staff finish_training [days] [player_id] [restaurant_id]`
- `staff list [player_id] [restaurant_id]`

Slot mapping used by staff commands:
- `0-5`: cooks
- `6-11`: waiters
- `12`: manager

## armsdealer

- `armsdealer market`