#pragma once

#include <array>
#include <string>
#include <vector>

#include "core/Tile.hpp"

// Starting situations for testing the UI: the first round of a game begins
// with chosen hands and draws, so rare situations (calls, kans, furiten, ...)
// can be reached on purpose.
namespace mj {

// A starting situation for the first round, written from the human's point of
// view. The opponents are meant to be CallingPlayers: they discard every tile
// they draw, so their draws are their discards.
struct Scenario {
    std::string name;
    // 13 tiles each, Tenhou notation, by position: you, right, across, left.
    std::array<std::string, 4> hands;
    // The first draws of each position, by position; may be empty.
    std::array<std::vector<std::string>, 4> draws;
};

const std::vector<Scenario>& scenarios();

// The start of the live wall for this scenario: the hands in deal order from
// the dealer (seat 0), then the draws in turn order (seat 0, 1, 2, 3, 0, ...)
// until the next drawer has no draw left. This assumes no calls while the draws
// last. Throws std::invalid_argument if a hand has not 13 tiles.
std::vector<Tile> scenarioWallStart(const Scenario& scenario, int humanSeat);

}  // namespace mj
