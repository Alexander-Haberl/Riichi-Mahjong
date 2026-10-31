#include <emscripten/bind.h>

#include "web/WebGame.hpp"

EMSCRIPTEN_BINDINGS(riichi) {
    emscripten::class_<mj::WebGame>("WebGame")
        .constructor<unsigned>()
        .function("start", &mj::WebGame::start)
        .function("takeSteps", &mj::WebGame::takeSteps)
        .function("pending", &mj::WebGame::pending)
        .function("submit", &mj::WebGame::submit)
        .function("result", &mj::WebGame::result);
}
