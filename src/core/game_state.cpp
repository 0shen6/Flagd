#include "core/game_state.h"

#include <algorithm>

#include "core/boardgen.h"

namespace flagd {

bool Player::has(RelicId r) const {
    return std::find(relics.begin(), relics.end(), r) != relics.end();
}

Player makePlayer(ClassId cls) {
    Player p;
    p.cls = cls;
    p.maxHp = classDef(cls).maxHp;
    p.hp = p.maxHp;
    p.items = {ItemId::Potion};
    return p;
}

Encounter makeEncounter(const balance::FloorParams& params, uint64_t seed) {
    Encounter e;
    e.params = params;
    e.seed = seed;
    e.board = Board(params.width, params.height);
    e.rng.reseed(deriveSeed(seed, 0xE7C0));
    return e;
}

bool addItem(Player& player, ItemId id) {
    if (int(player.items.size()) >= balance::kInventorySlots) return false;
    player.items.push_back(id);
    return true;
}

void gainXp(Player& player, int amount, std::vector<Event>& events) {
    amount = std::max(amount, amount * classDef(player.cls).xpPercent / 100);
    if (amount <= 0) return;
    player.xp += amount;
    events.push_back({EventType::XpGained, {}, amount});
    while (player.xp >= balance::xpToNext(player.level)) {
        player.xp -= balance::xpToNext(player.level);
        ++player.level;
        player.maxHp += balance::kLevelUpMaxHp;
        player.hp = player.maxHp;
        events.push_back({EventType::LevelUp, {}, player.level});
    }
}

void heal(Player& player, int amount, std::vector<Event>& events) {
    const int gained = std::min(amount, player.maxHp - player.hp);
    if (gained <= 0) return;
    player.hp += gained;
    events.push_back({EventType::Heal, {}, gained});
}

int rowThreat(const Board& b, int y) {
    int total = 0;
    for (const Monster& m : b.monsters())
        if (m.alive && std::any_of(m.cells.begin(), m.cells.end(), [&](Pos p) { return p.y == y; }))
            total += m.power;
    return total;
}

int colThreat(const Board& b, int x) {
    int total = 0;
    for (const Monster& m : b.monsters())
        if (m.alive && std::any_of(m.cells.begin(), m.cells.end(), [&](Pos p) { return p.x == x; }))
            total += m.power;
    return total;
}

int tenthStepCountdown(const Encounter& enc) {
    return balance::kTenthStepEvery - enc.revealClicks % balance::kTenthStepEvery;
}

namespace {

class Rules {
public:
    Rules(Player& pl, Encounter& enc, std::vector<Event>& ev) : pl_(pl), enc_(enc), b_(enc.board), ev_(ev) {}

    void apply(const Action& a) {
        switch (a.type) {
            case ActionType::Reveal: reveal(a.pos); break;
            case ActionType::CycleMark:
            case ActionType::SetMark: mark(a); break;
            case ActionType::UseItem: useItem(a.slot, a.pos); break;
            case ActionType::Descend: descend(); break;
        }
    }

private:
    void invalid(Pos p) { ev_.push_back({EventType::Invalid, p}); }

    void addGold(Pos p, int amount) {
        if (amount <= 0) return;
        pl_.gold += amount;
        pl_.goldEarned += amount;
        ev_.push_back({EventType::Gold, p, amount});
    }

    void damage(Pos p, int amount, bool ambush = false) {
        if (amount <= 0) return;
        pl_.hp -= amount;
        ev_.push_back({EventType::Damage, p, amount, ambush ? 1 : 0});
        if (pl_.hp <= 0) {
            pl_.hp = 0;
            enc_.lost = true;
            ev_.push_back({EventType::PlayerDied, p});
        }
    }

    // Collects what newly revealed cells hold.
    void pickUp(const std::vector<Pos>& cells) {
        for (Pos p : cells) {
            Cell& c = b_.at(p);
            ++pl_.tilesRevealed;
            if (c.gold) {
                int g = c.gold;
                if (pl_.has(RelicId::CoinMagnet)) g = (g * 3 + 1) / 2;
                c.gold = 0;
                addGold(p, g);
            }
            if (c.feature == Feature::Key) {
                c.feature = Feature::None;
                enc_.hasKey = true;
                ev_.push_back({EventType::KeyFound, p});
            }
            if (c.feature == Feature::Exit && !enc_.exitFound) {
                enc_.exitFound = true;
                ev_.push_back({EventType::ExitFound, p});
            }
        }
    }

    void generate(Pos first) {
        const int radius = pl_.has(RelicId::LuckyStart) ? 2 : 1;
        GeneratedBoard g = generateBoard(enc_.params, first, radius, enc_.seed);
        b_ = std::move(g.board);
        enc_.generated = true;
        enc_.hintCount = int(g.hints.size());
        pickUp(b_.revealSafe(first));
        for (Pos h : g.hints)
            if (!b_.at(h).revealed) pickUp(b_.revealSafe(h));
        if (!g.hints.empty()) ev_.push_back({EventType::HintsPlaced, first, int(g.hints.size())});
    }

    void fight(int monsterId, Pos p, bool deliberate, bool free) {
        Monster& m = b_.monsters()[size_t(monsterId)];
        const MonsterDef& def = monsterDef(m.kind);
        const bool flagged = b_.at(p).mark != Mark::None;
        int dmg = free ? 0 : def.hitCost * (deliberate ? 1 : balance::kAmbushMultiplier);
        if (dmg > 0 && pl_.has(RelicId::ThickSkin)) dmg = std::max(0, dmg - 1);
        if (dmg > 0 && pl_.shield > 0) {
            --pl_.shield;
            dmg = 0;
            ev_.push_back({EventType::ShieldBlock, p});
        }
        m.visible = true;
        damage(p, dmg, !deliberate);
        if (!pl_.alive()) return;

        --m.hp;
        if (flagged && !free && pl_.has(RelicId::BountyFlags)) addGold(p, balance::kBountyGold);
        gainXp(pl_, def.xp, ev_);
        if (m.hp > 0) {
            ev_.push_back({EventType::BossHit, p, m.hp});
            return;
        }
        m.alive = false;
        ++pl_.kills;
        ev_.push_back({EventType::Kill, p, 0, int(m.kind)});
        pickUp(b_.settleAfterKill(monsterId));
        if (def.boss) {
            enc_.won = true;
            ev_.push_back({EventType::BossDefeated, p});
        }
    }

    // A click (or chord) on a cell the player cannot see into.
    void revealHidden(Pos p, bool fromChord) {
        Cell& c = b_.at(p);
        if (c.buried) {
            pickUp(b_.revealSafe(p));
            return;
        }
        if (Monster* m = b_.livingMonsterAt(p)) {
            const bool deliberate = m->visible || c.mark != Mark::None;
            if (!deliberate && !fromChord) {
                ++enc_.revealClicks;
                if (pl_.has(RelicId::TenthStep) && enc_.revealClicks % balance::kTenthStepEvery == 0) {
                    m->visible = true;
                    ev_.push_back({EventType::Warded, p});
                    return;
                }
            }
            fight(c.monster, p, deliberate, false);
            return;
        }
        if (!fromChord) ++enc_.revealClicks;
        if (c.mark != Mark::None && pl_.has(RelicId::BountyFlags)) {
            ev_.push_back({EventType::WrongFlag, p});
            damage(p, 1);
            if (!pl_.alive()) return;
        }
        pickUp(b_.revealSafe(p));
    }

    // Opens every unmarked hidden neighbour when the marks and visible monsters
    // around p add up exactly to its number.
    bool chord(Pos p) {
        const int threat = b_.threatAt(p);
        int known = 0;
        int seen[9];
        int seenCount = 0;
        bool genericFlag = false;
        std::vector<Pos> targets;
        forNeighbors(b_.width(), b_.height(), p, [&](Pos n) {
            const Cell& c = b_.at(n);
            if (c.buried) {
                targets.push_back(n);
                return;
            }
            if (b_.isHidden(n)) {
                if (c.mark == Mark::Flag) genericFlag = true;
                else if (c.mark != Mark::None) known += markPower(c.mark);
                else targets.push_back(n);
                return;
            }
            const Monster* m = b_.livingMonsterAt(n);
            if (!m) return;
            for (int i = 0; i < seenCount; ++i)
                if (seen[i] == c.monster) return;
            seen[seenCount++] = c.monster;
            known += m->power;
        });
        if (targets.empty() || genericFlag || known != threat) {
            invalid(p);
            return false;
        }
        int opened = 0;
        for (Pos t : targets) {
            if (!pl_.alive() || enc_.won) break;
            if (!b_.isHidden(t) && !b_.at(t).buried) continue;
            revealHidden(t, true);
            ++opened;
        }
        if (pl_.alive() && pl_.has(RelicId::WideChord)) {
            forNeighbors(b_.width(), b_.height(), p, [&](Pos n) {
                if (chebyshev(n, p) == 2 && b_.isHidden(n) && !b_.livingMonsterAt(n))
                    pickUp(b_.revealSafe(n));
            }, 2);
        }
        if (opened > 0 && pl_.has(RelicId::ChordMiner)) addGold(p, 1);
        return true;
    }

    void openChest(Pos p) {
        if (!enc_.hasKey) {
            ev_.push_back({EventType::NeedKey, p});
            return;
        }
        enc_.hasKey = false;
        b_.at(p).chestOpen = true;
        ev_.push_back({EventType::ChestOpened, p});
        addGold(p, enc_.rng.range(10, 18) + 2 * enc_.params.depth);
        if (enc_.params.elite || enc_.params.boss) {
            std::vector<RelicId> pool;
            for (int r = 0; r < kRelicKinds; ++r)
                if (!pl_.has(RelicId(r))) pool.push_back(RelicId(r));
            if (pool.empty()) {
                addGold(p, 40);
            } else {
                const RelicId r = enc_.rng.pick(pool);
                pl_.relics.push_back(r);
                ev_.push_back({EventType::RelicGained, p, 0, int(r)});
            }
        } else if (enc_.rng.chance(0.65)) {
            const ItemId item = ItemId(enc_.rng.range(0, kItemKinds - 1));
            if (addItem(pl_, item)) {
                ev_.push_back({EventType::ItemGained, p, 0, int(item)});
            } else {
                ev_.push_back({EventType::InventoryFull, p, 0, int(item)});
                addGold(p, itemDef(item).price / 2);
            }
        }
    }

    void endTurn() {
        ++enc_.turn;
        if (!pl_.alive()) return;
        if (!enc_.cleared && !enc_.params.boss && b_.allSafeRevealed()) {
            enc_.cleared = true;
            const int bonus = balance::clearBonus(enc_.params.depth);
            pl_.gold += bonus;
            pl_.goldEarned += bonus;
            ev_.push_back({EventType::BoardCleared, {}, bonus});
        }
        if (enc_.params.boss && !enc_.won && enc_.turn % balance::kBossBurrowEvery == 0) burrow();
    }

    // The Burrower re-covers one revealed number. The cell stays known-safe
    // (drawn as dirt), so only the number is hidden until dug up again.
    void burrow() {
        std::vector<Pos> candidates;
        for (int y = 0; y < b_.height(); ++y)
            for (int x = 0; x < b_.width(); ++x) {
                Pos p{x, y};
                const Cell& c = b_.at(p);
                if (c.revealed && c.feature == Feature::None && b_.threatAt(p) > 0) candidates.push_back(p);
            }
        if (candidates.empty()) return;
        const Pos p = enc_.rng.pick(candidates);
        b_.at(p).revealed = false;
        b_.at(p).buried = true;
        ev_.push_back({EventType::Burrow, p});
    }

    void reveal(Pos p) {
        if (!b_.inBounds(p)) return invalid(p);
        if (!enc_.generated) {
            generate(p);
            endTurn();
            return;
        }
        Cell& c = b_.at(p);
        if (c.revealed) {
            if (c.feature == Feature::Chest && !c.chestOpen) return openChest(p);
            if (c.feature == Feature::Exit) return invalid(p);  // use Descend
            if (chord(p)) endTurn();
            return;
        }
        revealHidden(p, false);
        endTurn();
    }

    void mark(const Action& a) {
        if (!enc_.generated || !b_.inBounds(a.pos) || !b_.isHidden(a.pos)) return invalid(a.pos);
        Cell& c = b_.at(a.pos);
        c.mark = a.type == ActionType::CycleMark ? nextMark(c.mark) : a.mark;
    }

    void descend() {
        if (!enc_.exitFound && !enc_.won) return invalid({});
        enc_.descended = true;
        ev_.push_back({EventType::Descend});
    }

    void useItem(int slot, Pos p) {
        if (slot < 0 || slot >= int(pl_.items.size())) return invalid(p);
        const ItemId id = pl_.items[size_t(slot)];
        if (itemDef(id).targeted && (!enc_.generated || !b_.inBounds(p))) return invalid(p);
        bool takesTurn = false;
        switch (id) {
            case ItemId::Scanner:
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        Pos q{p.x + dx, p.y + dy};
                        if (!b_.inBounds(q) || !(b_.isHidden(q) || b_.at(q).buried)) continue;
                        if (Monster* m = b_.livingMonsterAt(q)) m->visible = true;
                        else pickUp(b_.revealSafe(q));
                    }
                takesTurn = true;
                break;
            case ItemId::Defuser:
                if (b_.livingMonsterAt(p)) {
                    fight(b_.at(p).monster, p, true, true);
                } else if (b_.isHidden(p) || b_.at(p).buried) {
                    pickUp(b_.revealSafe(p));
                } else {
                    return invalid(p);
                }
                takesTurn = true;
                break;
            case ItemId::Flare:
                if (std::find(enc_.flareRows.begin(), enc_.flareRows.end(), p.y) == enc_.flareRows.end())
                    enc_.flareRows.push_back(p.y);
                if (std::find(enc_.flareCols.begin(), enc_.flareCols.end(), p.x) == enc_.flareCols.end())
                    enc_.flareCols.push_back(p.x);
                break;
            case ItemId::Shield:
                ++pl_.shield;
                break;
            case ItemId::Potion:
                if (pl_.hp >= pl_.maxHp) return invalid(p);
                heal(pl_, balance::kPotionHeal, ev_);
                break;
            case ItemId::Count:
                return invalid(p);
        }
        pl_.items.erase(pl_.items.begin() + slot);
        ev_.push_back({EventType::ItemUsed, p, 0, int(id)});
        if (takesTurn) endTurn();
    }

    Player& pl_;
    Encounter& enc_;
    Board& b_;
    std::vector<Event>& ev_;
};

} // namespace

std::vector<Event> applyAction(Player& player, Encounter& enc, const Action& action) {
    std::vector<Event> events;
    if (enc.lost || enc.descended || !player.alive()) return events;
    Rules(player, enc, events).apply(action);
    return events;
}

} // namespace flagd
