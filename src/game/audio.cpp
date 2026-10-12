#include "game/audio.h"

#include <raylib.h>

#include <array>
#include <cmath>
#include <vector>

namespace flagd::audio {

namespace {

constexpr int kRate = 22050;

enum class Wave { Square, Triangle, Noise };

struct Note {
    Wave wave;
    float f0, f1;  // start and end frequency (Hz)
    float seconds;
    float volume;
};

std::array<Sound, size_t(Sfx::Count)> g_sounds{};
std::array<std::vector<short>, size_t(Sfx::Count)> g_pcm;
bool g_ready = false;
bool g_muted = false;

std::vector<short> synth(std::initializer_list<Note> notes) {
    std::vector<short> out;
    uint32_t noise = 0x1234567u;
    float noiseVal = 0;
    for (const Note& n : notes) {
        const int frames = int(n.seconds * kRate);
        double phase = 0;
        for (int i = 0; i < frames; ++i) {
            const float t = float(i) / float(frames);
            const float f = n.f0 + (n.f1 - n.f0) * t;
            phase += f / kRate;
            phase -= std::floor(phase);
            float v = 0;
            switch (n.wave) {
                case Wave::Square: v = phase < 0.5 ? 1.0f : -1.0f; break;
                case Wave::Triangle: v = float(4.0 * std::fabs(phase - 0.5) - 1.0); break;
                case Wave::Noise:
                    // Hold each random value for a stretch: lower f = rumblier noise.
                    if (i % std::max(1, int(kRate / std::max(f, 1.0f))) == 0) {
                        noise ^= noise << 13;
                        noise ^= noise >> 17;
                        noise ^= noise << 5;
                        noiseVal = float(noise & 0xFFFF) / 32767.5f - 1.0f;
                    }
                    v = noiseVal;
                    break;
            }
            const float attack = std::min(1.0f, float(i) / (0.004f * kRate));
            const float env = attack * (1.0f - t) * (1.0f - t * 0.3f);
            out.push_back(short(v * env * n.volume * 32767.0f));
        }
    }
    return out;
}

void make(Sfx id, std::initializer_list<Note> notes) {
    auto& pcm = g_pcm[size_t(id)];
    pcm = synth(notes);
    ::Wave w{};
    w.frameCount = unsigned(pcm.size());
    w.sampleRate = kRate;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = pcm.data();
    g_sounds[size_t(id)] = LoadSoundFromWave(w);  // copies the samples
}

} // namespace

void init() {
    InitAudioDevice();
    g_ready = IsAudioDeviceReady();
    if (!g_ready) return;
    using W = Wave;
    make(Sfx::Reveal, {{W::Square, 880, 1250, 0.035f, 0.12f}});
    make(Sfx::Flag, {{W::Triangle, 620, 480, 0.05f, 0.35f}});
    make(Sfx::Coin, {{W::Square, 1320, 1320, 0.04f, 0.12f}, {W::Square, 1760, 1760, 0.08f, 0.12f}});
    make(Sfx::Error, {{W::Square, 160, 150, 0.06f, 0.12f}, {W::Square, 120, 110, 0.08f, 0.12f}});
    make(Sfx::Heal, {{W::Triangle, 523, 523, 0.06f, 0.35f}, {W::Triangle, 659, 659, 0.06f, 0.35f},
                     {W::Triangle, 784, 784, 0.1f, 0.35f}});
    make(Sfx::Hit, {{W::Noise, 3000, 1500, 0.06f, 0.35f}, {W::Square, 220, 110, 0.1f, 0.15f}});
    make(Sfx::Kill, {{W::Square, 440, 880, 0.07f, 0.13f}, {W::Triangle, 880, 990, 0.08f, 0.35f}});
    make(Sfx::Burrow, {{W::Noise, 400, 150, 0.3f, 0.4f}});
    make(Sfx::Descend, {{W::Triangle, 660, 180, 0.4f, 0.4f}});
    make(Sfx::LevelUp, {{W::Square, 523, 523, 0.07f, 0.12f}, {W::Square, 659, 659, 0.07f, 0.12f},
                        {W::Square, 784, 784, 0.07f, 0.12f}, {W::Square, 1046, 1046, 0.16f, 0.12f}});
    make(Sfx::Ambush, {{W::Noise, 2500, 600, 0.12f, 0.5f}, {W::Square, 180, 55, 0.22f, 0.18f}});
    make(Sfx::Explosion, {{W::Noise, 1200, 60, 0.7f, 0.6f}});
    make(Sfx::Victory, {{W::Square, 523, 523, 0.1f, 0.12f}, {W::Square, 659, 659, 0.1f, 0.12f},
                        {W::Square, 784, 784, 0.1f, 0.12f}, {W::Square, 1046, 1046, 0.1f, 0.12f},
                        {W::Triangle, 1046, 1046, 0.4f, 0.4f}});
    make(Sfx::Death, {{W::Triangle, 400, 80, 0.7f, 0.45f}});
}

void shutdown() {
    if (!g_ready) return;
    for (size_t i = 1; i < g_sounds.size(); ++i)
        if (g_sounds[i].frameCount > 0) UnloadSound(g_sounds[i]);
    CloseAudioDevice();
    g_ready = false;
}

void play(Sfx s) {
    if (!g_ready || g_muted || s == Sfx::None) return;
    const Sound& snd = g_sounds[size_t(s)];
    if (snd.frameCount > 0) PlaySound(snd);
}

void toggleMute() { g_muted = !g_muted; }
bool muted() { return g_muted; }

} // namespace flagd::audio
