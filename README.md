# Flagd

A 2D pixel-art minesweeper roguelite in C++20 with raylib.

Numbers show the **total power** of the monsters around a tile, not a mine count. Mark monsters, fight them for XP, level up to heal, and descend eight floors to a boss. A solver checks every board, including the ones with twists, so you never have to guess.

## Build (Windows, MSYS2 UCRT64)

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/flagd.exe
```

The first configure downloads raylib 5.5 and doctest. The build copies the sprite sheet to `build/assets/sheet.png`. Progress is saved to `%APPDATA%\Flagd\save.txt`.

Tests:

```bash
ctest --test-dir build --output-on-failure
./build/flagd_tests.exe --no-skip -tc="stress*"            # 7,000 boards, every floor and twist
./build/flagd_tests.exe --no-skip -tc="autoplay: every*"   # bot win rate per class and boss
```

## Controls

| Input | Action |
|---|---|
| Left click | Reveal a tile, attack a marked or visible monster, chord a number, open a chest, take the stairs |
| Right click | Cycle marks: flag, P1, P2, P3, P5, none |
| `1` `2` `3` `5` / `F` / `0` | Set a power mark, a flag, or clear the mark on the hovered tile |
| Middle click / `Space` | Same as left click on the hovered tile |
| `Q` `W` `E` `R` | Use or select an item (Esc cancels) |
| `M` | Mute or unmute sound |
| `F11` | Toggle fullscreen |

Hover over a number to see what is already accounted for. Its neighbours are outlined, which helps on hex and wrap-around boards.

## Rules in short

- **Threat numbers:** each number is the sum of the power of the living monsters next to that tile. When a monster dies, the numbers around it drop.
- **Fights:**
  - Clicking a monster you marked (or one that is visible) costs its power in HP and gives XP.
  - Stumbling into an unmarked monster is an **ambush** and costs double.
- **Level ups** raise max HP and heal you. Time your big fights for just before one.
- **Chords:** click a number whose marks and visible monsters add up exactly, and its other neighbours open.

### Monsters

| Monster | Power | Twist |
|---|---|---|
| Slime, Bat, Skeleton, Spider | 1, 2, 3, 5 | none |
| Powder Keg | 2 | Unmarked, it explodes for 4 damage and exposes everything around it. Marked, it defuses for 1. |
| Plague Polyp | 1 | Ambush poisons you: 1 HP per turn for 3 turns. |
| Cursed Idol | 2 | Ambush curses the board: XP is halved. |
| Sleeping Golem | 3 | Free to slay once nothing around it is hidden. |
| Wisp | 2 | Always visible. It drifts one tile per turn, lighting its trail, and the numbers shift with it. |

**Bosses** (one per run, chosen by seed):
- **Burrower:** buries a revealed number every 4 turns.
- **Fog Wyrm:** every number is fogged into a range. Each hit clears the air around it.
- **Hive Mother:** births a Wisp every 5 turns.

### Classes

| Class | HP | XP | Ability | Unlock |
|---|---|---|---|---|
| Warrior | 12 | 125% | Plain and sturdy | Start |
| Sapper | 11 | 110% | 2 Defusers; explosions half damage; kegs defuse free | Reach floor 4 |
| Seer | 10 | 130% | Coloured pips show which kinds lurk around each number; starts with a Scanner | Win a run |
| Gambler | 11 | 115% | Unproven reveals that turn out safe heal 1 HP; ambushes cost triple | Earn 150 gold in a run |

### Board twists

Twists appear from floor 3 and show as a blue badge on the map:
- **Wrap-around:** the edges connect.
- **Hex grid:** six neighbours per tile.
- **Fog:** numbers show a range, like 4-6.
- **Liars:** purple numbers are off by 1 or 2 and should be ignored.

### The run and beyond

- **The map:** 8 floors of boards, elite boards (a relic in the chest), shops, rest sites, treasure and **events** (6 kinds, each a choice with a cost).
- **Heat 1–5** is opt-in difficulty, unlocked one level per win. Each level stacks a new rule and adds +15% embers per level.
- **Embers** carry over between runs. Spend them at the **Camp** on small upgrades (max HP, gold, a starting Potion, a starting Flare) and on 5 extra relics for the pool.
- **The Codex** records every monster, relic and twist you have met.

## Layout

- `src/core/` holds the game rules and depends on nothing but the standard library:
  - `board`: cells, topologies (square, wrap, hex), threat sums, fog and liar views, and flood fill.
  - `solver`: sound deduction over threat sums, in two stages: cheap local windows, then exact components combined against the floor census.
  - `boardgen`: places a board, simulates no-guess play, retries layouts, and adds hint tiles if needed.
  - `game_state`: `applyAction()` turns an action into events. All board rules live here, including traits, statuses, wandering, bosses and class abilities.
  - `map` / `run`: map generation, shops, rest sites, treasure and events.
  - `meta`: embers, unlocks, upgrades, codex, and plain-text save files.
  - `content/`: the data tables. `balance.h` holds every tunable number.
- `src/game/` is the raylib layer: a 640×360 canvas scaled up in whole steps, the screens, and synthesized sound effects (`audio.cpp`, no audio files).
- `tests/` covers unit tests, a brute-force solver soundness check over every topology, fog and liars, and autoplay bots for balance.

`flagd.exe --shots <dir>` plays a scripted tour and saves a screenshot of every screen and board twist. In that mode it keeps its save file in `<dir>`.

## Art

[Kenney 1-Bit Pack](https://kenney.nl/assets/1-bit-pack) (CC0). The monochrome sheet is tinted in code.
