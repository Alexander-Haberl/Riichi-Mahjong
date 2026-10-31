#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mj {

enum class Suit : uint8_t { Man, Pin, Sou, Honor };

// In tile order, so the value + 1 is the Tenhou number ("1z" = East, "7z" = Red).
enum class Honor : uint8_t { East, South, West, North, White, Green, Red };

inline constexpr int kNumTileKinds = 34;
inline constexpr int kNumTiles = 136;

// Number of tiles of each of the 34 kinds. Most rules logic works on this.
using TileCounts = std::array<uint8_t, kNumTileKinds>;

// A tile: one of the 34 kinds plus a flag for red fives (aka-dora).
// Kinds: 0-8 = 1-9m, 9-17 = 1-9p, 18-26 = 1-9s, 27-30 = E S W N, 31-33 = white green red.
// Copies are not distinguished; the wall decides how many of each kind exist
// and which fives are red.
class Tile {
public:
    constexpr Tile() = default;
    constexpr explicit Tile(uint8_t kind, bool red = false) : kind_(kind), red_(red) {}

    // number is 1-9 for suits and 1-7 for honors (E S W N white green red),
    // matching Tenhou notation ("5m", "7z"). red is only valid for suited fives.
    static constexpr Tile make(Suit suit, int number, bool red = false) {
        return Tile(static_cast<uint8_t>(static_cast<int>(suit) * 9 + number - 1), red);
    }
    static constexpr Tile honor(Honor h) { return make(Suit::Honor, static_cast<int>(h) + 1); }

    constexpr uint8_t kind() const { return kind_; }
    constexpr bool isRed() const { return red_; }
    constexpr Suit suit() const { return static_cast<Suit>(kind_ / 9); }
    constexpr int number() const { return kind_ % 9 + 1; }
    // Precondition: isHonor().
    constexpr Honor asHonor() const { return static_cast<Honor>(kind_ - 27); }

    constexpr bool isHonor() const { return kind_ >= 27; }
    constexpr bool isWind() const { return kind_ >= 27 && kind_ <= 30; }
    constexpr bool isDragon() const { return kind_ >= 31; }
    constexpr bool isTerminal() const { return !isHonor() && (number() == 1 || number() == 9); }
    constexpr bool isTerminalOrHonor() const { return isHonor() || isTerminal(); }
    constexpr bool isSimple() const { return !isTerminalOrHonor(); }

    // Same kind, ignoring the red flag. Use this for rules logic
    // (waits, furiten, calls); == also compares the red flag.
    constexpr bool sameKind(Tile other) const { return kind_ == other.kind_; }

    // The dora this tile points to when it is the dora indicator (9 -> 1, N -> E, red -> white).
    Tile doraFromIndicator() const;

    // Red fives print as "0m" / "0p" / "0s".
    std::string toString() const;

    // Exact comparison: kind, then red flag (a red five sorts after the normal one).
    constexpr auto operator<=>(const Tile&) const = default;

private:
    uint8_t kind_ = 0;
    bool red_ = false;
};

// Parses Tenhou-style notation, e.g. "123m406p789s1122z". '0' is a red five.
// Throws std::invalid_argument on malformed input.
std::vector<Tile> parseTiles(std::string_view text);
// Same notation; red fives count as normal fives.
TileCounts parseCounts(std::string_view text);

int totalTiles(const TileCounts& counts);
std::string toString(const TileCounts& counts);

}  // namespace mj
