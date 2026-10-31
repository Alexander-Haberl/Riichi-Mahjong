#include "game/LegalActions.hpp"

#include <algorithm>

#include "rules/Decomposer.hpp"

namespace mj{
    namespace {
        // One copy of each distinct tile (red fives separate), in hand order.
        std::vector<Tile> distinctTiles(const std::vector<Tile>& tiles) {
            std::vector<Tile> result;
            for (Tile t : tiles) {
                if (std::find(result.begin(), result.end(), t) == result.end()) result.push_back(t);
            }
            return result;
        }

        std::vector<Tile> tilesOfKind(const Hand& hand, Tile kind) {
            std::vector<Tile> result;
            for (Tile t : hand.concealed()) {
                if (t.sameKind(kind)) result.push_back(t);
            }
            return result;
        }

        // A closed kan during riichi must use the drawn tile and keep the waits.
        bool riichiKanKeepsWaits(const PlayerState& player, Tile kind) {
            Tile drawn = player.tileDrawn();
            if (!drawn.sameKind(kind)) return false;
            TileCounts before = player.hand.counts();
            --before[drawn.kind()];
            TileCounts after = player.hand.counts();
            after[kind.kind()] = 0;
            return waits(before) == waits(after);
        }

        WinContext baseContext(const RoundState& state, int seat) {
            const PlayerState& player = state.player(seat);
            WinContext ctx;
            ctx.hand = player.hand;
            ctx.riichi = player.riichi;
            ctx.doubleRiichi = player.doubleRiichi;
            ctx.ippatsu = player.ippatsu;
            ctx.roundWind = state.roundWind;
            ctx.seatWind = player.seatWind;
            ctx.doraIndicators = state.wall.doraIndicators();
            if (player.riichi) ctx.uraDoraIndicators = state.wall.uraDoraIndicators();
            return ctx;
        }

        // Discards that leave a closed hand tenpai, if riichi is allowed at all.
        std::vector<Action> computeRiichiActions(const RoundState& state, const RuleSet& rules) {
            const PlayerState& player = state.currentPlayer();
            const Hand& hand = player.hand;
            const bool enoughPoints = !rules.riichiNeeds1000Points || player.score >= 1000;
            // At least 4 tiles left, so the player still has a draw of their own.
            if (!hand.isClosed() || player.riichi || !enoughPoints || state.wall.remaining() < 4) return {};

            std::vector<Action> result;
            const TileCounts counts = hand.counts();
            for (Tile t : distinctTiles(hand.concealed())) {
                TileCounts rest = counts;
                --rest[t.kind()];
                if (isTenpai(rest)) result.push_back({ActionType::Riichi, t});
            }
            return result;
        }

        std::vector<Action> computeTsumoActions(const RoundState& state, const Scorer& scorer) {
            if (!scorer.score(tsumoContext(state))) return {};
            return {Action{ActionType::Tsumo, state.currentPlayer().tileDrawn()}};
        }

        // Closed kans of four concealed tiles, and kans made by adding a tile to a pon.
        std::vector<Action> computeKanActions(const RoundState& state) {
            // A kan needs a rinshan tile, and is not allowed on the last tile of the wall.
            if (state.wall.rinshanRemaining() <= 0 || state.wall.remaining() == 0) return {};

            const PlayerState& player = state.currentPlayer();
            const Hand& hand = player.hand;
            std::vector<Action> result;

            const TileCounts counts = hand.counts();
            for (size_t kind = 0; kind < counts.size(); ++kind) {
                if (counts[kind] != 4) continue;
                Tile kanKind(static_cast<uint8_t>(kind));
                if (player.riichi && !riichiKanKeepsWaits(player, kanKind)) continue;
                result.push_back({ActionType::ClosedKan, kanKind, tilesOfKind(hand, kanKind)});
            }

            for (const Meld& meld : hand.melds()) {
                if (meld.type != MeldType::Pon) continue;
                // Three of the four tiles are in the pon, so at most one can match.
                for (Tile t : hand.concealed()) {
                    if (t.sameKind(meld.baseTile())) result.push_back({ActionType::KanFromPon, t});
                }
            }
            return result;
        }

        // Kyuushu kyuuhai: first own turn, no call yet, nine kinds of terminals and honors.
        bool abortiveDraw(const RoundState& state, const RuleSet& rules) {
            const PlayerState& player = state.currentPlayer();
            if (!rules.kyuushuKyuuhai || state.anyCall || !player.firstTurn()) return false;

            const TileCounts counts = player.hand.counts();
            int kinds = 0;
            for (size_t kind = 0; kind < counts.size(); ++kind) {
                if (counts[kind] > 0 && Tile(static_cast<uint8_t>(kind)).isTerminalOrHonor()) ++kinds;
            }
            return kinds >= 9;
        }

        // In riichi only the drawn tile; right after a chi / pon everything except the kuikae tiles.
        std::vector<Action> computeDiscards(const RoundState& state, const RuleSet& rules) {
            const PlayerState& player = state.currentPlayer();
            if (player.riichi) return {Action{ActionType::Discard, player.tileDrawn()}};

            std::vector<Tile> discards = distinctTiles(player.hand.concealed());
            if (!state.tileDraw && state.lastCall && !rules.allowKuikae) {
                for (Tile forbidden : kuikaeForbidden(*state.lastCall)) {
                    std::erase_if(discards, [forbidden](Tile t) { return t.sameKind(forbidden); });
                }
            }

            std::vector<Action> result;
            for (Tile t : discards) result.push_back({ActionType::Discard, t});
            return result;
        }

        // All ways to take n tiles of a kind from the hand. They differ only in
        // how many red fives they use. Assumes at most 1 red five.
        std::vector<std::vector<Tile>> pickTiles(const Hand& hand, Tile kind, int n) {
            int reds = 0;
            int normals = 0;
            for (Tile t : tilesOfKind(hand, kind)) {
                if (t.isRed()) ++reds;
                else ++normals;
            }
            std::vector<std::vector<Tile>> result;
            if (reds >= 1 && normals >= n - 1){
                std::vector<Tile> pick{Tile(kind.kind(), true)};
                pick.insert(pick.end(), static_cast<size_t>(n-1), Tile(kind.kind()));
                result.push_back(std::move(pick));
            }
            if (normals >= n){
                std::vector<Tile> pick(static_cast<size_t>(n), Tile(kind.kind()));
                result.push_back(std::move(pick));
            }
            return result;
        }

        // Whether the hand still has a legal discard after making this chi / pon.
        bool leavesDiscard(const Hand& hand, const Meld& call, const RuleSet& rules) {
            if (rules.allowKuikae) return true;
            TileCounts rest = hand.counts();
            for (Tile t : call.handTiles) --rest[t.kind()];           // tiles that go into the meld
            for (Tile t : kuikaeForbidden(call)) rest[t.kind()] = 0;  // kinds that may not be discarded
            return totalTiles(rest) > 0;
        }

        bool canRon(const RoundState& state, int seat, Tile tile, ClaimSource source, const Scorer& scorer) {
            if (state.player(seat).isFuriten()) return false;
            return scorer.score(ronContext(state, seat, tile, source)).has_value();
        }

        // Chi, pon and open kan: only on a discard, never in riichi, never on the last discard.
        bool canCall(const RoundState& state, int seat, ClaimSource source) {
            return source == ClaimSource::Discard && !state.player(seat).riichi && state.wall.remaining() > 0;
        }

        std::vector<Action> computePonActions(const RoundState& state, int seat, Tile tile, int fromSeat,
                                              ClaimSource source, const RuleSet& rules) {
            if (!canCall(state, seat, source)) return {};
            const Hand& hand = state.player(seat).hand;
            std::vector<Action> result;
            for (auto& pick : pickTiles(hand, tile, 2)) {
                Meld call{MeldType::Pon, pick, tile, fromSeat};
                if (leavesDiscard(hand, call, rules)) result.push_back({ActionType::Pon, tile, pick});
            }
            return result;
        }

        // Uses all three matching tiles, so there is at most one open kan.
        std::optional<Action> computeKanCallAction(const RoundState& state, int seat, Tile tile, ClaimSource source) {
            if (!canCall(state, seat, source) || state.wall.rinshanRemaining() <= 0) return std::nullopt;
            std::vector<Tile> matching = tilesOfKind(state.player(seat).hand, tile);
            if (matching.size() != 3) return std::nullopt;
            return Action{ActionType::OpenKan, tile, matching};
        }

        std::vector<Action> computeChiActions(const RoundState& state, int seat, Tile tile, int fromSeat,
                                      ClaimSource source, const RuleSet& rules) {
            const bool fromLeft = relativeSeat(seat, fromSeat) == 3;
            if (!canCall(state, seat, source) || !fromLeft || tile.isHonor()) return {};

            // Offsets of the two hand tiles from the called tile: called is the high, middle or low end.
            constexpr std::array<std::pair<int, int>, 3> kShapes{{{-2, -1}, {-1, 1}, {1, 2}}};

            const Hand& hand = state.player(seat).hand;
            const int n = tile.number();
            std::vector<Action> result;
            for (auto [offsetA, offsetB] : kShapes) {
                if (n + offsetA < 1 || n + offsetB > 9) continue;
                for (auto& a : pickTiles(hand, Tile::make(tile.suit(), n + offsetA), 1)) {
                    for (auto& b : pickTiles(hand, Tile::make(tile.suit(), n + offsetB), 1)) {
                        Meld call{MeldType::Chi, {a[0], b[0]}, tile, fromSeat};
                        if (leavesDiscard(hand, call, rules)) result.push_back({ActionType::Chi, tile, call.handTiles});
                    }
                }
            }
            return result;
        }
    }

    WinContext ronContext(const RoundState& state, int seat, Tile tile, ClaimSource source) {
        WinContext ctx = baseContext(state, seat);
        ctx.hand.add(tile);
        ctx.winTile = tile;
        ctx.houtei = source == ClaimSource::Discard && state.wall.remaining() == 0;
        ctx.chankan = source == ClaimSource::KanFromPon;
        return ctx;
    }

    WinContext tsumoContext(const RoundState& state) {
        WinContext ctx = baseContext(state, state.current);
        ctx.winTile = state.currentPlayer().tileDrawn();
        ctx.tsumo = true;
        ctx.rinshan = state.rinshan;
        ctx.haitei = state.wall.remaining() == 0 && !state.rinshan;
        return ctx;
    }

    std::vector<Tile> kuikaeForbidden(const Meld& call) {
        if (!call.calledTile) return {};
        Tile called = *call.calledTile;
        std::vector<Tile> forbidden{Tile(called.kind())};
        if (call.type == MeldType::Chi) {
            Tile low = call.baseTile();
            if (called.sameKind(low) && low.number() <= 6) {
                forbidden.emplace_back(static_cast<uint8_t>(low.kind() + 3));  // called 3 with 45: also 6
            } else if (called.number() == low.number() + 2 && low.number() >= 2) {
                forbidden.emplace_back(static_cast<uint8_t>(low.kind() - 1));  // called 6 with 45: also 3
            }
        }
        return forbidden;
    }

    std::vector<Action> legalTurnActions(const RoundState& state, const RuleSet& rules,
                                    const Scorer& scorer){
        std::vector<Action> result;
        if (state.tileDraw){
            for (auto a : computeRiichiActions(state, rules)){
                result.push_back(a);
            }

            for (auto a : computeTsumoActions(state, scorer)){
                result.push_back(a);
            }

            for (auto a : computeKanActions(state)){
                result.push_back(a);
            }

            if (abortiveDraw(state, rules)){
                result.push_back(Action{ActionType::KyuushuKyuuhai});
            }
            // TODO discarding the drawn tile is a different action than discarding the same tile from the inside of the hand
        }
        for (auto a : computeDiscards(state, rules)){
            result.push_back(a);
        }
        return result;
    }

    std::vector<Action> legalReactions(const RoundState& state, int seat, Tile tile,
                                    int fromSeat, ClaimSource source, const RuleSet& rules,
                                    const Scorer& scorer){
        std::vector<Action> result;
        if (seat == fromSeat){
            return result;
        }

        if (canRon(state, seat, tile, source, scorer)){
            result.push_back(Action{ActionType::Ron, tile});
        }

        for (auto a : computePonActions(state, seat, tile, fromSeat, source, rules)){
            result.push_back(a);
        }

        std::optional<Action> kanOption = computeKanCallAction(state, seat, tile, source);
        if(kanOption){
            result.push_back(*kanOption);
        }

        for(auto a : computeChiActions(state, seat, tile, fromSeat, source, rules)){
            result.push_back(a);
        }
        if(!result.empty()){
            result.push_back(Action{ActionType::Pass});
        }
        
        return result;

    }
}
