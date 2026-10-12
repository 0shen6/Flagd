#pragma once
// Deterministic RNG (xoshiro256**) so a run seed reproduces every board.
#include <cstdint>
#include <utility>
#include <vector>

namespace flagd {

inline uint64_t splitmix64(uint64_t& x) {
    uint64_t z = (x += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// Derives an independent seed from a parent seed and a salt (e.g. a map node id).
inline uint64_t deriveSeed(uint64_t seed, uint64_t salt) {
    uint64_t x = seed ^ (salt * 0xD1B54A32D192ED03ull);
    splitmix64(x);
    return splitmix64(x);
}

class Rng {
public:
    explicit Rng(uint64_t seed = 1) { reseed(seed); }

    void reseed(uint64_t seed) {
        for (auto& s : s_) s = splitmix64(seed);
    }

    uint64_t next() {
        const uint64_t result = rotl(s_[1] * 5, 7) * 9;
        const uint64_t t = s_[1] << 17;
        s_[2] ^= s_[0];
        s_[3] ^= s_[1];
        s_[1] ^= s_[2];
        s_[0] ^= s_[3];
        s_[2] ^= t;
        s_[3] = rotl(s_[3], 45);
        return result;
    }

    // Uniform integer in [lo, hi] (inclusive).
    int range(int lo, int hi) {
        if (hi <= lo) return lo;
        const uint64_t span = uint64_t(hi - lo) + 1;
        return lo + int(next() % span);
    }

    double unit() { return double(next() >> 11) * 0x1.0p-53; }
    bool chance(double p) { return unit() < p; }

    template <class T>
    void shuffle(std::vector<T>& v) {
        for (size_t i = v.size(); i > 1; --i) {
            size_t j = size_t(next() % i);
            std::swap(v[i - 1], v[j]);
        }
    }

    template <class T>
    const T& pick(const std::vector<T>& v) {
        return v[size_t(next() % v.size())];
    }

private:
    static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
    uint64_t s_[4]{};
};

} // namespace flagd
