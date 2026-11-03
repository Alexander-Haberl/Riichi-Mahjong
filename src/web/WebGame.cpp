#include "web/WebGame.hpp"

#include <optional>
#include <stdexcept>
#include <vector>

#include "players/CallingPlayer.hpp"
#include "players/ShantenPlayer.hpp"
#include "web/Json.hpp"
#include "web/Scenarios.hpp"

namespace mj {

namespace {

constexpr int kHumanPlayer = 0;

std::array<std::unique_ptr<IPlayer>, 3> makeAis(bool calling) {
    std::array<std::unique_ptr<IPlayer>, 3> ais;
    for (auto& ai : ais) {
        if (calling) {
            ai = std::make_unique<CallingPlayer>();
        } else {
            ai = std::make_unique<ShantenPlayer>();
        }
    }
    return ais;
}

}  // namespace

WebGame::WebGame(unsigned seed, int scenario, bool callingAis)
    : ais_(makeAis(callingAis)),
      game_({nullptr, ais_[0].get(), ais_[1].get(), ais_[2].get()}, standardScorer(webRules()), seed, kHumanPlayer) {
    if (scenario < 0) return;
    const Scenario& chosen = scenarios().at(static_cast<size_t>(scenario));
    game_.stackFirstWall(scenarioWallStart(chosen, game_.seatOf(kHumanPlayer)));
}

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

std::string scenarioNames() {
    nlohmann::json names = nlohmann::json::array();
    for (const Scenario& scenario : scenarios()) names.push_back(scenario.name);
    return names.dump();
}

}  // namespace mj
