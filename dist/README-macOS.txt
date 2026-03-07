Pizza Legacy v0.0.1 - macOS Release (Apple Silicon)
====================================================

Pizza Legacy is an open-source reimplementation of the 1994 DOS game
Pizza Connection/Pizza Tycoon.


FIRST LAUNCH - GATEKEEPER WARNING
----------------------------------
macOS will refuse to open Pizza Legacy because it is not signed with an
Apple developer certificate. To fix this, open Terminal and run:

    xattr -cr "/path/to/Pizza Legacy.app"

Then double-click the app as normal. You only need to do this once.
If you don't know the path you can just type `xattr -cr` and then drag the
Pizza Legacy application to the terminal to insert the path for you.


REQUIREMENTS
------------
The game requires the asset files from the original Pizza Tycoon /
Pizza Connection. Point the game at your installation directory on
first launch (the folder containing PT.EXE).

The original game (Pizza Connection) is available on GOG and Steam,
frequently on sale for under a euro.


STATUS
------
Alpha. Expect rough edges and missing features. Development is active.
See https://pizzalegacy.nl for current status.


LICENSE
-------
Pizza Legacy is licensed under the GNU General Public License v3.0.
Source code: https://codeberg.org/cowomaly/pizzalegacy

The "all dependencies" line in INCLUDED FILES keeps it simple — unlike Windows
you don't need to list individual dylibs since they're hidden inside the
bundle.
