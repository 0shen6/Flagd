#pragma once
#include <algorithm>
#include <cstdlib>
#include <vector>

namespace flagd {

struct Pos {
    int x = 0;
    int y = 0;
    bool operator==(const Pos&) const = default;
};

inline int chebyshev(Pos a, Pos b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

template <class T>
class Grid {
public:
    Grid() = default;
    Grid(int w, int h, const T& fill = T{}) : w_(w), h_(h), data_(size_t(w) * size_t(h), fill) {}

    int width() const { return w_; }
    int height() const { return h_; }
    bool inBounds(Pos p) const { return p.x >= 0 && p.y >= 0 && p.x < w_ && p.y < h_; }

    T& operator[](Pos p) { return data_[index(p)]; }
    const T& operator[](Pos p) const { return data_[index(p)]; }
    size_t index(Pos p) const { return size_t(p.y) * size_t(w_) + size_t(p.x); }
    Pos posOf(size_t i) const { return {int(i % size_t(w_)), int(i / size_t(w_))}; }
    size_t size() const { return data_.size(); }

private:
    int w_ = 0;
    int h_ = 0;
    std::vector<T> data_;
};

// Calls f(Pos) for each in-bounds cell within Chebyshev `radius` of p, excluding p.
template <class F>
void forNeighbors(int w, int h, Pos p, F&& f, int radius = 1) {
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx == 0 && dy == 0) continue;
            Pos n{p.x + dx, p.y + dy};
            if (n.x >= 0 && n.y >= 0 && n.x < w && n.y < h) f(n);
        }
}

} // namespace flagd
