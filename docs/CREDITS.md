# Credits

Pizza Legacy is a reimplementation of the 1994 game Pizza Tycoon.

Copyright (C) 2026 Cowomaly <https://pizzalegacy.nl/>

## Original game

Pizza Tycoon (released as Pizza Connection in Germany) was created by
Cybernetic Corporation and published by Software 2000 (German) and MicroProse
(English) in 1994. Thanks to the original developers for a game worth
reimplementing three decades later.

Thanks also to [pizzatycoon.org](https://www.pizzatycoon.org/) for keeping the
game's documentation available online.

## Open-source libraries

This project is made possible by the following open-source libraries.
Full license texts are available in the `LICENSES/` directory.

### System dependencies (linked libraries)

#### SDL3

Usage: Platform abstraction (Graphics, Input, Audio, Window management)

    Copyright: Copyright (c) 1997-2026 Sam Lantinga
    License:   zlib License (see LICENSES/zlib.txt)

#### libADLMIDI

Usage: Software OPL3 FM synthesis for CMF music

    Copyright: Copyright (c) 2015-2024 Vitaly Novichkov
    Licenses:  LGPL-3.0-or-later (see LICENSES/LGPL-3.0-or-later.txt)
               GPL-3.0-or-later (see LICENSES/GPL-3.0-or-later.txt)

#### libpng

Usage: PNG support for screenshots

    Copyright: Copyright (c) 1995-2026 The PNG Reference Entities
    License:   libpng License 2.0 (see LICENSES/libpng.txt)

#### zlib

Usage:     Compression for JSON save files

    Copyright: Copyright (C) 1995-2026 Jean-loup Gailly and Mark Adler
    License:   zlib License (see LICENSES/zlib-zlib.txt)

### Bundled libraries

Source code for these libraries is included in the `third_party/` directory of
the [source tree](https://codeberg.org/cowomaly/pizzalegacy/).

#### nlohmann/json

Usage: JSON production/consumption for save games

    Copyright: Copyright (c) 2013-2024 Niels Lohmann
    License:   MIT (see LICENSES/MIT-nlohmann.txt)

#### UTF8-CPP

Usage: Handling of UTF-8 encoded strings and conversions

    Copyright: Copyright 2006 Nemanja Trifunovic
    License:   Boost Software License 1.0 (see LICENSES/BSL-1.0.txt)

#### tinyfiledialogs

Usage: Native OS dialogs for asset directory selection

    Copyright: Copyright (c) 2014-2024 Guillaume Vareille
    License:   zlib License (see LICENSES/zlib.txt)

#### magic\_enum

Usage: String conversion of enums for debugging

    Copyright: Copyright (c) 2019-2024 Daniil Goncharov
    License:   MIT (see LICENSES/MIT-magic-enum.txt)
