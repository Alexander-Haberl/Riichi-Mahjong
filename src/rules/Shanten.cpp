#include "rules/Shanten.hpp"

#include <algorithm>

namespace mj::shanten {

namespace {

// Depth-first search over all ways to split the hand into
// sets (mentsu), partial sets (taatsu), one pair and isolated tiles.
// Fast enough for an AI that evaluates a few hundred hands per decision;
// replace with a lookup-table implementation if it ever becomes a bottleneck.
struct RegularSearch {
    TileCounts c;
    int best = 8;

    void run(size_t i, int sets, int partials, bool pair) {
        while (i < c.size() && c[i] == 0) ++i;
        if (i == c.size()) {
            // At most 4 sets + partial sets can be useful.
            int usefulPartials = std::min(partials, 4 - sets);
            best = std::min(best, 8 - 2 * sets - usefulPartials - (pair ? 1 : 0));
            return;
        }

        bool suited = i < 27;
        int number = static_cast<int>(i % 9) + 1;

        if (c[i] >= 3) {
            c[i] -= 3;
            run(i, sets + 1, partials, pair);
            c[i] += 3;
        }
        if (suited && number <= 7 && c[i + 1] && c[i + 2]) {
            --c[i]; --c[i + 1]; --c[i + 2];
            run(i, sets + 1, partials, pair);
            ++c[i]; ++c[i + 1]; ++c[i + 2];
        }
        if (c[i] >= 2) {
            c[i] -= 2;
            if (!pair) run(i, sets, partials, true);
            run(i, sets, partials + 1, pair);
            c[i] += 2;
        }
        if (suited && number <= 8 && c[i + 1]) {  // ryanmen / penchan
            --c[i]; --c[i + 1];
            run(i, sets, partials + 1, pair);
            ++c[i]; ++c[i + 1];
        }
        if (suited && number <= 7 && c[i + 2]) {  // kanchan
            --c[i]; --c[i + 2];
            run(i, sets, partials + 1, pair);
            ++c[i]; ++c[i + 2];
        }
        // Treat one copy as an isolated tile.
        --c[i];
        run(i, sets, partials, pair);
        ++c[i];
    }
};

constexpr uint8_t kTerminalsAndHonors[] = {0, 8, 9, 17, 18, 26, 27, 28, 29, 30, 31, 32, 33};

}  // namespace

int regular(const TileCounts& concealed, int meldCount) {
    RegularSearch search{concealed};
    search.run(0, meldCount, 0, false);
    return search.best;
}

int chiitoitsu(const TileCounts& concealed) {
    int pairs = 0;
    int kinds = 0;
    for (uint8_t n : concealed) {
        if (n >= 1) ++kinds;
        if (n >= 2) ++pairs;
    }
    // Four of a kind counts as only one pair, so missing kinds cost extra.
    return 6 - pairs + std::max(0, 7 - kinds);
}

int kokushi(const TileCounts& concealed) {
    int kinds = 0;
    bool pair = false;
    for (uint8_t id : kTerminalsAndHonors) {
        if (concealed[id] >= 1) ++kinds;
        if (concealed[id] >= 2) pair = true;
    }
    return 13 - kinds - (pair ? 1 : 0);
}

int calc(const TileCounts& concealed, int meldCount) {
    int result = regular(concealed, meldCount);
    if (meldCount == 0) {
        result = std::min({result, chiitoitsu(concealed), kokushi(concealed)});
    }
    return result;
}

int ukeire(const TileCounts& concealed, int meldCount, const TileCounts& visible) {
    const int current = calc(concealed, meldCount);
    TileCounts counts = concealed;
    int tiles = 0;
    for (size_t kind = 0; kind < counts.size(); ++kind) {
        if (visible[kind] >= 4) continue;
        ++counts[kind];
        if (calc(counts, meldCount) < current) tiles += 4 - visible[kind];
        --counts[kind];
    }
    return tiles;
}

}  // namespace mj::shanten
