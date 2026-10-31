#pragma once

#include <nlohmann/json.hpp>

#include "game/Game.hpp"
#include "game/GameStep.hpp"

// The engine's types as JSON for the UI. Tiles are Tenhou strings ("5m", "0p",
// "7z"), enums are their names as strings, and an empty optional is null. The
// fields are described in docs/ui-design.md.
namespace mj {

nlohmann::json toJson(const GameStep& step);
nlohmann::json toJson(const Decision& decision);
nlohmann::json toJson(const GameResult& result);

}  // namespace mj
