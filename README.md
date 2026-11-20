# Riichi Mahjong

This is an implementation of Japanese Riichi Mahjong, as a singleplayer game with 3 AI opponents. Coded under major use of AI with human oversight.

The game runs in the browser, using JavaScript. A python webserver serves the JavaScript files, tile images and riichi engine once, after which the game is played locally in the browser.
The exact default rule set used in this game can be found in src/scoring/RuleSet.hpp.

Currently the AI only optimizes for Riichi, with better AI coming in the future.

The engine and AI are written in C++ and compiled to WebAssembly.

**Requirements for building:**
- CMake 3.20 (or higher), C++20 compiler, Emscripten (web assembly), Python 3 (web server)

**Dependencies** (Fetched during the first build run):
- Catch2 (for native build), JSON library (https://github.com/nlohmann/json)

**How to use:**
- Build the site (result lands in build-web/site):   
  ```sh
  emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
  cmake --build build-web
  ```

- Build and serve the site (requires Emscripten, either `emcmake` on the `PATH` or `EMSDK_DIR` set to the emsdk folder):   
  ```sh
  ./serve.sh [port]
  ```

  Builds the web-game and launches a python webserver on http://127.0.0.1:8000 by default, or on the given port. Now head there to play.

- Native build of engine and testing:
  ```sh
  cmake -S . -B build
  cmake --build build
  ./build/riichi_tests
  ```   
  or sanitized build   
  ```sh
  cmake -S . -B build-asan -DRIICHI_SANITIZE=ON
  cmake --build build-asan
  ./build-asan/riichi_tests
  ```

**Project layout:**
- src/ contains the C++ engine and AIs
    - core/ contains core types and functionality (Tiles, Hand, Hand decomposition, ...)
    - game/ contains general game flow, legal actions computation and gamestate structs
    - scoring/ contains yaku and scoring, and the ruleset
    - web/ wraps main gameloop to expose functionality to JavaScript
    - ai/ contains AIs
- web/ contains JavaScript, HTML, CSS and the tile images
- tests/ various tests
- docs/ documentation on UI-design and yaku

