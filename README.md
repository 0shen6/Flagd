# Flagd

A 2D pixel-art minesweeper roguelite in C++20 with raylib.

Numbers show the **total power** of the monsters around a tile, not a mine count. Mark monsters, fight them for XP, level up to heal, and descend floor by floor to the Burrower. A solver checks every board, so you never have to guess.

## Build (Windows, MSYS2 UCRT64)

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/flagd.exe
```

The first configure downloads raylib 5.5 and doctest. The build copies the sprite sheet to `build/assets/sheet.png`.

Tests:

```bash
ctest --test-dir build --output-on-failure
./build/flagd_tests.exe --no-skip -tc="stress*"   # 7,000-board no-guess check
```

## Controls

| Input | Action |
|---|---|
| Left click | Reveal a tile, attack a marked or visible monster, chord a number, open a chest, take the stairs |
| Right click | Cycle marks: flag, P1, P2, P3, P5, none |
| `1` `2` `3` `5` / `F` / `0` | Set a power mark, a flag, or clear the mark on the hovered tile |
| Middle click / `Space` | Same as left click on the hovered tile |
| `Q` `W` `E` `R` | Use or select an item (Esc cancels) |
| `F11` | Toggle fullscreen |

## Rules in short

- **Threat numbers:** each number is the sum of the power of the living monsters next to that tile. When a monster dies, the numbers around it drop.
- **Fights:**
  - Clicking a monster you marked (or one that is visible) costs its power in HP and gives XP.
  - Stumbling into an unmarked monster is an **ambush** and costs double.
- **Level ups** raise max HP and heal you fully. Time your big fights for just before one.
- **Chords:** click a number whose marks and visible monsters add up exactly, and its other neighbours open.
- **The run:** 8 floors on a branching map of boards, elite boards, shops, rest sites and treasure. The Burrower boss waits at the bottom and re-buries a revealed tile every 4 turns.

## Layout

- `src/core/` holds the game rules and depends on nothing but the standard library:
  - `board`: cells, threat sums and flood fill.
  - `solver`: exact deduction over threat sums and the floor census.
  - `boardgen`: places a board, then simulates no-guess play and retries layouts.
  - `game_state`: `applyAction()` turns an action into events. All rules live here.
  - `map` / `run`: map generation, shops, rest sites and treasure.
  - `content/`: the monster, item, relic and class tables, plus `balance.h` with every tunable number.
- `src/game/` is the raylib layer. It draws a 640×360 canvas scaled up in whole steps, and holds the screens.
- `tests/` covers unit tests, a brute-force solver soundness check, and autoplay bots that check balance.
  - `test_balance.cpp` prints the win rate of a careful bot and a sloppy one. Use it when tuning `balance.h`.

`flagd.exe --shots <dir>` plays a scripted sequence and saves a screenshot of each screen. It's useful for checking visuals.

## Art

[Kenney 1-Bit Pack](https://kenney.nl/assets/1-bit-pack) (CC0). The monochrome sheet is tinted in code.
