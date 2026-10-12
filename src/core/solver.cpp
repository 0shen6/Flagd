#include "core/solver.h"

#include <algorithm>
#include <numeric>
#include <set>
#include <unordered_map>

namespace flagd {

namespace {

// How many monsters of each census power an assignment uses, packed 8 bits
// per power (at most 4 distinct powers). Lanes never overflow because each
// lane stays <= its census count.
using Usage = uint32_t;

// The hidden neighbours of a known number must sum into [lo, hi]
// (lo == hi unless the number is fogged).
struct Constraint {
    int lo = 0;
    int hi = 0;
    std::vector<int> vars;
};

struct Census {
    std::vector<int> powers;  // distinct powers still hidden
    std::vector<int> counts;  // how many monsters of each power are hidden
    int total = 0;
    int maxPower = 0;
};

Census hiddenCensus(const Board& b) {
    Census c;
    const auto counts = b.livingCounts(true);
    for (int k = 0; k < kMonsterKinds; ++k) {
        if (counts[size_t(k)] == 0) continue;
        const int pw = monsterDef(MonsterKind(k)).power;
        auto it = std::find(c.powers.begin(), c.powers.end(), pw);
        if (it == c.powers.end()) {
            c.powers.push_back(pw);
            c.counts.push_back(counts[size_t(k)]);
        } else {
            c.counts[size_t(it - c.powers.begin())] += counts[size_t(k)];
        }
        c.total += counts[size_t(k)];
        c.maxPower = std::max(c.maxPower, pw);
    }
    return c;
}

int lane(Usage u, size_t k) { return int((u >> (8 * k)) & 0xFFu); }

int monstersIn(Usage u) {
    int n = 0;
    for (size_t k = 0; k < 4; ++k) n += lane(u, k);
    return n;
}

bool withinCensus(Usage u, const Census& c) {
    for (size_t k = 0; k < c.counts.size(); ++k)
        if (lane(u, k) > c.counts[k]) return false;
    return true;
}

// Pairwise sums of two usage sets, dropping any that exceed the census.
std::vector<Usage> combine(const std::vector<Usage>& a, const std::vector<Usage>& b, const Census& c) {
    std::vector<Usage> out;
    for (Usage x : a)
        for (Usage y : b)
            if (withinCensus(x + y, c)) out.push_back(x + y);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

int findRoot(std::vector<int>& parent, int x) {
    while (parent[size_t(x)] != x) x = parent[size_t(x)] = parent[size_t(parent[size_t(x)])];
    return x;
}

// Enumerates every assignment of one connected group of frontier cells. For
// each distinct census usage it records, per variable, the set of values seen
// (bit 0 = safe, bit k+1 = census.powers[k]).
class ComponentSearch {
public:
    using Masks = std::vector<unsigned>;  // indexed by position in vars

    ComponentSearch(const std::vector<Constraint>& cons, const std::vector<int>& vars,
                    const std::vector<std::vector<int>>& varCons, const Census& census, long budget)
        : cons_(cons), vars_(vars), varCons_(varCons), census_(census), budget_(budget),
          sum_(cons.size(), 0), open_(cons.size(), 0), used_(census.powers.size(), 0),
          value_(vars.size(), 0) {
        for (int v : vars_)
            for (int c : varCons_[size_t(v)]) open_[size_t(c)] = int(cons_[size_t(c)].vars.size());
    }

    // Returns false if the node budget ran out.
    bool run() {
        recurse(0);
        return !outOfBudget_;
    }

    std::unordered_map<Usage, Masks> byUsage;

private:
    bool feasible(int v) const {
        for (int c : varCons_[size_t(v)]) {
            const Constraint& con = cons_[size_t(c)];
            const int s = sum_[size_t(c)];
            if (s > con.hi || s + open_[size_t(c)] * census_.maxPower < con.lo) return false;
        }
        return true;
    }

    void apply(int v, int power, int dir) {
        for (int c : varCons_[size_t(v)]) {
            sum_[size_t(c)] += power * dir;
            open_[size_t(c)] -= dir;
        }
    }

    void recordLeaf() {
        Usage u = 0;
        for (size_t k = 0; k < used_.size(); ++k) u |= Usage(used_[k]) << (8 * k);
        Masks& m = byUsage[u];
        if (m.empty()) m.assign(vars_.size(), 0u);
        for (size_t i = 0; i < vars_.size(); ++i) m[i] |= 1u << unsigned(value_[i]);
    }

    void recurse(size_t i) {
        if (outOfBudget_) return;
        if (--budget_ < 0) {
            outOfBudget_ = true;
            return;
        }
        if (i == vars_.size()) {
            recordLeaf();
            return;
        }
        const int v = vars_[i];
        value_[i] = 0;
        apply(v, 0, 1);
        if (feasible(v)) recurse(i + 1);
        apply(v, 0, -1);
        for (size_t k = 0; k < census_.powers.size(); ++k) {
            if (used_[k] >= census_.counts[k]) continue;
            const int pw = census_.powers[k];
            value_[i] = int(k) + 1;
            ++used_[k];
            apply(v, pw, 1);
            if (feasible(v)) recurse(i + 1);
            apply(v, pw, -1);
            --used_[k];
        }
        value_[i] = 0;
    }

    const std::vector<Constraint>& cons_;
    const std::vector<int>& vars_;
    const std::vector<std::vector<int>>& varCons_;
    const Census& census_;
    long budget_;
    std::vector<int> sum_;
    std::vector<int> open_;
    std::vector<int> used_;
    std::vector<int> value_;
    bool outOfBudget_ = false;
};

// Enumerates a small window of variables against only the constraints lying
// entirely inside it. Dropping the other constraints is a relaxation, so any
// deduction made here is sound. Stops as soon as every variable is ambiguous.
// Returns true only for a complete enumeration: an early stop leaves value
// sets partial, and intersecting partial sets would be unsound.
class WindowSearch {
public:
    WindowSearch(const std::vector<Constraint>& cons, const std::vector<int>& winCons, const std::vector<int>& winVars,
                 const Census& census, std::vector<int>& localIndex, long budget)
        : census_(census), budget_(budget), nVars_(int(winVars.size())) {
        for (int i = 0; i < nVars_; ++i) localIndex[size_t(winVars[size_t(i)])] = i;
        varCons_.resize(size_t(nVars_));
        for (int c : winCons) {
            Local lc{cons[size_t(c)].lo, cons[size_t(c)].hi, {}};
            for (int v : cons[size_t(c)].vars) lc.vars.push_back(localIndex[size_t(v)]);
            for (int v : lc.vars) varCons_[size_t(v)].push_back(int(local_.size()));
            local_.push_back(std::move(lc));
        }
        for (int v : winVars) localIndex[size_t(v)] = -1;
        sum_.assign(local_.size(), 0);
        open_.resize(local_.size());
        for (size_t c = 0; c < local_.size(); ++c) open_[c] = int(local_[c].vars.size());
        used_.assign(census.powers.size(), 0);
        value_.assign(size_t(nVars_), 0);
        masks.assign(size_t(nVars_), 0u);
    }

    bool run() {
        recurse(0);
        return !outOfBudget_ && !allAmbiguous_;
    }

    std::vector<unsigned> masks;  // by window position

private:
    struct Local {
        int lo, hi;
        std::vector<int> vars;
    };

    bool feasible(int v) const {
        for (int c : varCons_[size_t(v)]) {
            const int s = sum_[size_t(c)];
            if (s > local_[size_t(c)].hi || s + open_[size_t(c)] * census_.maxPower < local_[size_t(c)].lo) return false;
        }
        return true;
    }

    void apply(int v, int power, int dir) {
        for (int c : varCons_[size_t(v)]) {
            sum_[size_t(c)] += power * dir;
            open_[size_t(c)] -= dir;
        }
    }

    void recurse(int i) {
        if (outOfBudget_ || allAmbiguous_) return;
        if (--budget_ < 0) {
            outOfBudget_ = true;
            return;
        }
        if (i == nVars_) {
            for (int v = 0; v < nVars_; ++v) {
                const unsigned before = masks[size_t(v)];
                masks[size_t(v)] |= 1u << unsigned(value_[size_t(v)]);
                const bool was = (before & 1u) && (before & ~1u);
                const bool now = (masks[size_t(v)] & 1u) && (masks[size_t(v)] & ~1u);
                if (now && !was) ++ambiguous_;
            }
            allAmbiguous_ = ambiguous_ == nVars_;
            return;
        }
        value_[size_t(i)] = 0;
        apply(i, 0, 1);
        if (feasible(i)) recurse(i + 1);
        apply(i, 0, -1);
        for (size_t k = 0; k < census_.powers.size(); ++k) {
            if (used_[k] >= census_.counts[k]) continue;
            const int pw = census_.powers[k];
            value_[size_t(i)] = int(k) + 1;
            ++used_[k];
            apply(i, pw, 1);
            if (feasible(i)) recurse(i + 1);
            apply(i, pw, -1);
            --used_[k];
        }
        value_[size_t(i)] = 0;
    }

    const Census& census_;
    long budget_;
    int nVars_;
    std::vector<Local> local_;
    std::vector<std::vector<int>> varCons_;
    std::vector<int> sum_;
    std::vector<int> open_;
    std::vector<int> used_;
    std::vector<int> value_;
    int ambiguous_ = 0;
    bool allAmbiguous_ = false;
    bool outOfBudget_ = false;
};

constexpr int kWindowVars = 14;
constexpr long kWindowBudget = 6000;
constexpr int kMaxExactVars = 40;

} // namespace

SolverResult solve(const Board& b, SolveMode mode, long nodeBudget) {
    SolverResult out;
    const int w = b.width();
    const int h = b.height();
    const Census census = hiddenCensus(b);

    std::vector<Pos> hidden;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (b.isHidden({x, y})) hidden.push_back({x, y});
    if (hidden.empty()) return out;
    if (census.total == 0) {
        out.safe = hidden;
        return out;
    }

    // Variables are hidden cells next to a known number; each known number
    // (revealed, or buried but remembered) is one sum constraint. Liar tiles
    // carry no reliable information and are skipped.
    Grid<int> varIndex(w, h, -1);
    std::vector<Pos> varPos;
    std::vector<Constraint> cons;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const Pos p{x, y};
            const Cell& c = b.at(p);
            if (!(c.revealed || c.buried)) continue;  // a Wisp standing on a number does not hide it
            const NumberView nv = b.view(p);
            if (nv.liar) continue;
            Constraint con;
            con.lo = nv.lo;
            con.hi = nv.hi;
            int seen[8];
            int seenCount = 0;
            b.forNeighbors(p, [&](Pos n) {
                if (b.isHidden(n)) {
                    if (varIndex[n] < 0) {
                        varIndex[n] = int(varPos.size());
                        varPos.push_back(n);
                    }
                    con.vars.push_back(varIndex[n]);
                    return;
                }
                const Monster* m = b.livingMonsterAt(n);
                if (!m || !m->visible) return;
                const int id = b.at(n).monster;
                for (int i = 0; i < seenCount; ++i)
                    if (seen[i] == id) return;
                seen[seenCount++] = id;
                con.lo -= m->power;
                con.hi -= m->power;
            });
            con.lo = std::max(con.lo, 0);
            if (!con.vars.empty()) cons.push_back(std::move(con));
        }

    const int nVars = int(varPos.size());
    const int interior = int(hidden.size()) - nVars;
    std::vector<std::vector<int>> varCons(static_cast<size_t>(nVars));
    std::vector<int> parent(static_cast<size_t>(nVars));
    std::iota(parent.begin(), parent.end(), 0);
    for (size_t ci = 0; ci < cons.size(); ++ci) {
        const auto& vs = cons[ci].vars;
        for (int v : vs) varCons[size_t(v)].push_back(int(ci));
        for (size_t i = 1; i < vs.size(); ++i) {
            const int a = findRoot(parent, vs[0]);
            const int c = findRoot(parent, vs[i]);
            if (a != c) parent[size_t(a)] = c;
        }
    }

    // Per-variable verdicts: bit 0 = may be safe, bit k+1 = may hold power k.
    // Every fully enumerated window narrows a variable (AND of verdicts).
    std::vector<unsigned> verdict(static_cast<size_t>(nVars), ~0u);

    // Stage 1: local windows around each constraint.
    {
        std::set<std::vector<int>> done;
        std::vector<int> localIndex(static_cast<size_t>(nVars), -1);
        std::vector<char> inWin(static_cast<size_t>(nVars), 0);
        for (size_t ci = 0; ci < cons.size(); ++ci) {
            std::vector<int> win = cons[ci].vars;
            for (int v : win) inWin[size_t(v)] = 1;
            for (int v : cons[ci].vars)
                for (int c2 : varCons[size_t(v)]) {
                    int extra = 0;
                    for (int u : cons[size_t(c2)].vars) extra += !inWin[size_t(u)];
                    if (int(win.size()) + extra > kWindowVars) continue;
                    for (int u : cons[size_t(c2)].vars)
                        if (!inWin[size_t(u)]) {
                            inWin[size_t(u)] = 1;
                            win.push_back(u);
                        }
                }
            std::vector<int> key = win;
            std::sort(key.begin(), key.end());
            if (!done.insert(key).second) {
                for (int v : win) inWin[size_t(v)] = 0;
                continue;
            }
            std::vector<int> winCons;
            for (int v : win)
                for (int c2 : varCons[size_t(v)]) {
                    bool inside = true;
                    for (int u : cons[size_t(c2)].vars) inside = inside && inWin[size_t(u)];
                    if (inside && std::find(winCons.begin(), winCons.end(), c2) == winCons.end()) winCons.push_back(c2);
                }
            for (int v : win) inWin[size_t(v)] = 0;
            WindowSearch ws(cons, winCons, win, census, localIndex, kWindowBudget);
            if (!ws.run()) continue;
            for (size_t i = 0; i < win.size(); ++i)
                if (ws.masks[i] != 0u) verdict[size_t(win[i])] &= ws.masks[i];
        }
    }
    auto emit = [&](const std::vector<unsigned>& masks) {
        for (int v = 0; v < nVars; ++v) {
            const unsigned m = masks[size_t(v)];
            if (m == 0u || m == ~0u) continue;  // inconsistent or unknown
            if (m == 1u) {
                out.safe.push_back(varPos[size_t(v)]);
            } else if ((m & 1u) == 0u) {
                int power = 0;
                for (size_t k = 0; k < census.powers.size(); ++k)
                    if (m == (1u << unsigned(k + 1))) power = census.powers[k];
                out.monsters.push_back(varPos[size_t(v)]);
                out.monsterPower.push_back(power);
            }
        }
    };
    if (mode == SolveMode::Fast) {
        bool any = false;
        for (unsigned m : verdict) any = any || m == 1u || (m != ~0u && m != 0u && (m & 1u) == 0u);
        if (any) {
            emit(verdict);
            return out;
        }
    }

    // Stage 2: group variables into components, in BFS order so constraints close early.
    std::vector<std::vector<int>> comps;
    {
        std::vector<char> visited(static_cast<size_t>(nVars), 0);
        for (int start = 0; start < nVars; ++start) {
            if (visited[size_t(start)]) continue;
            std::vector<int> order{start};
            visited[size_t(start)] = 1;
            for (size_t qi = 0; qi < order.size(); ++qi)
                for (int c : varCons[size_t(order[qi])])
                    for (int v : cons[size_t(c)].vars)
                        if (!visited[size_t(v)]) {
                            visited[size_t(v)] = 1;
                            order.push_back(v);
                        }
            comps.push_back(std::move(order));
        }
    }

    std::vector<ComponentSearch> searches;
    searches.reserve(comps.size());
    std::vector<bool> solved(comps.size(), false);
    for (size_t i = 0; i < comps.size(); ++i) {
        searches.emplace_back(cons, comps[i], varCons, census, nodeBudget);
        solved[i] = int(comps[i].size()) <= kMaxExactVars && searches[i].run();
    }

    // A component's usage is globally feasible if the other components can
    // complete it without exceeding the census, with the leftover monsters
    // fitting in the interior cells. Unsolved components block this step.
    const bool exact = std::all_of(solved.begin(), solved.end(), [](bool s) { return s; });
    auto fits = [&](Usage total) {
        return withinCensus(total, census) && census.total - monstersIn(total) <= interior;
    };
    auto usagesOf = [&](size_t i) {
        std::vector<Usage> us;
        for (const auto& [u, m] : searches[i].byUsage) us.push_back(u);
        return us;
    };

    std::vector<unsigned> masks(static_cast<size_t>(nVars), 0u);
    for (size_t i = 0; i < comps.size(); ++i) {
        if (!solved[i]) continue;
        std::vector<Usage> others{0};
        if (exact)
            for (size_t j = 0; j < comps.size(); ++j)
                if (j != i) others = combine(others, usagesOf(j), census);
        for (const auto& [u, m] : searches[i].byUsage) {
            bool ok = !exact;
            for (Usage o : others) ok = ok || fits(o + u);
            if (!ok) continue;
            for (size_t k = 0; k < comps[i].size(); ++k) masks[size_t(comps[i][k])] |= m[k];
        }
    }

    // Combine both stages: solved components are exact, windows can only narrow.
    for (size_t i = 0; i < comps.size(); ++i)
        if (solved[i])
            for (int v : comps[i]) verdict[size_t(v)] &= masks[size_t(v)];
    emit(verdict);

    // Interior cells (no adjacent number) through the global monster count.
    if (exact && interior > 0) {
        std::vector<Usage> totals{0};
        for (size_t i = 0; i < comps.size(); ++i) totals = combine(totals, usagesOf(i), census);
        bool allZero = true;
        bool allFull = true;
        bool any = false;
        for (Usage t : totals) {
            if (!fits(t)) continue;
            any = true;
            const int left = census.total - monstersIn(t);
            allZero = allZero && left == 0;
            allFull = allFull && left == interior;
        }
        if (any && (allZero || allFull)) {
            for (Pos p : hidden) {
                if (varIndex[p] >= 0) continue;
                if (allZero) {
                    out.safe.push_back(p);
                } else {
                    out.monsters.push_back(p);
                    out.monsterPower.push_back(census.powers.size() == 1 ? census.powers[0] : 0);
                }
            }
        }
    }
    return out;
}

} // namespace flagd
