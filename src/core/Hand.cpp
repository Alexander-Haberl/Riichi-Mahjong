#include "core/Hand.hpp"

#include <algorithm>
#include <stdexcept>

namespace mj {

namespace {

bool removeFrom(std::vector<Tile>& tiles, Tile tile) {
    auto it = std::find(tiles.begin(), tiles.end(), tile);
    if (it == tiles.end()) return false;
    tiles.erase(it);
    return true;
}

}  // namespace

Hand::Hand(std::vector<Tile> concealed) : concealed_(std::move(concealed)) {}

void Hand::add(Tile tile) {
    concealed_.push_back(tile);
}

bool Hand::remove(Tile tile) {
    return removeFrom(concealed_, tile);
}

Tile Hand::lastTile() const {
    if (concealed_.empty()) throw std::logic_error("no concealed tile in hand");
    return concealed_.back(); 
}

int Hand::count(Tile kind) const {
    return static_cast<int>(std::count_if(concealed_.begin(), concealed_.end(),
                                          [kind](Tile t) { return t.sameKind(kind); }));
}

void Hand::addMeld(const Meld& meld) {
    std::vector<Tile> remaining = concealed_;
    for (Tile t : meld.handTiles) {
        if (!removeFrom(remaining, t)) throw std::logic_error("meld tile not in hand: " + t.toString());
    }
    concealed_ = std::move(remaining);
    melds_.push_back(meld);
}

void Hand::addKanToPon(Tile tile) {
    auto pon = std::find_if(melds_.begin(), melds_.end(), [tile](const Meld& m) {
        return m.type == MeldType::Pon && m.baseTile().sameKind(tile);
    });
    if (pon == melds_.end()) throw std::logic_error("no pon to add to: " + tile.toString());
    if (!remove(tile)) throw std::logic_error("kan tile not in hand: " + tile.toString());

    pon->type = MeldType::OpenKan;
    pon->handTiles.push_back(tile);
    pon->kanFromPon = true;
}

TileCounts Hand::counts() const {
    TileCounts counts{};
    for (Tile t : concealed_) ++counts[t.kind()];
    return counts;
}

bool Hand::isClosed() const {
    return std::none_of(melds_.begin(), melds_.end(), [](const Meld& m) { return m.isOpen(); });
}

std::string Hand::toString() const {
    std::string result = mj::toString(counts());
    for (const Meld& m : melds_) {
        result += " [";
        for (Tile t : m.tiles()) result += t.toString();
        result += "]";
    }
    return result;
}

}  // namespace mj
