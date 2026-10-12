#pragma once
// Retro sound effects synthesized at startup (no audio files needed).
#include <cstdint>

namespace flagd {

// Ordered by priority: when one action triggers several, the highest plays.
enum class Sfx : uint8_t {
    None,
    Reveal,
    Flag,
    Coin,
    Error,
    Heal,
    Hit,
    Kill,
    Burrow,
    Descend,
    LevelUp,
    Ambush,
    Explosion,
    Victory,
    Death,
    Count
};

namespace audio {
void init();
void shutdown();
void play(Sfx s);
void toggleMute();
bool muted();
} // namespace audio

} // namespace flagd
