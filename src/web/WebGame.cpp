#include "web/WebGame.hpp"

#include <optional>
#include <stdexcept>
#include <vector>

#include "web/Json.hpp"

namespace mj {

namespace {

constexpr int kHumanPlayer = 0;

}  // namespace

WebGame::WebGame(unsigned seed)
    : game_({nullptr, &ais_[0], &ais_[1], &ais_[2]}, standardScorer(webRules()), seed, kHumanPlayer) {}

void WebGame::start() {
    game_.start();
}

std::string WebGame::takeSteps() {
    nlohmann::json steps = nlohmann::json::array();
    for (const GameStep& step : game_.takeSteps()) steps.push_back(toJson(step));
    return steps.dump();
}

std::string WebGame::pending() const {
    const std::optional<Decision> decision = game_.pending();
    return decision ? toJson(*decision).dump() : "null";
}

void WebGame::submit(int actionIndex) {
    const std::optional<Decision> decision = game_.pending();
    if (!decision) throw std::logic_error("no decision is pending");
    if (actionIndex < 0 || actionIndex >= static_cast<int>(decision->legal.size())) {
        throw std::logic_error("no legal action with this index");
    }
    game_.submit(decision->legal[static_cast<size_t>(actionIndex)]);
}

std::string WebGame::result() const {
    return toJson(game_.result()).dump();
}

}  // namespace mj
