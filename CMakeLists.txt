cmake_minimum_required(VERSION 3.20)
project(RiichiMahjong VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(RIICHI_BUILD_TESTS "Build unit tests" ON)
option(RIICHI_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# Set before any target (Catch2 included) so everything is built the same way.
if(RIICHI_SANITIZE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
        add_link_options(-fsanitize=address,undefined)
    else()
        message(WARNING "RIICHI_SANITIZE is only supported with GCC and Clang")
    endif()
endif()

# --- Engine library (no UI, no I/O) -----------------------------------------
add_library(riichi_engine
    src/core/Tile.cpp
    src/core/Hand.cpp
    src/core/Wall.cpp
    src/rules/Shanten.cpp
    src/rules/Decomposer.cpp
    src/rules/Scorer.cpp
    src/rules/Yaku.cpp
    src/game/LegalActions.cpp
    src/game/RoundController.cpp
    src/game/Game.cpp
    src/players/ShantenPlayer.cpp
)
target_include_directories(riichi_engine PUBLIC src)
target_compile_options(riichi_engine PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion -Wshadow>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# --- Game executable --------------------------------------------------------
add_executable(riichi src/main.cpp)
target_link_libraries(riichi PRIVATE riichi_engine)

# --- Tests ------------------------------------------------------------------
if(RIICHI_BUILD_TESTS)
    find_package(Catch2 3 QUIET)
    if(NOT Catch2_FOUND)
        include(FetchContent)
        FetchContent_Declare(Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG        v3.7.1
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(Catch2)
        list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    endif()

    enable_testing()
    add_executable(riichi_tests
        tests/test_tile.cpp
        tests/test_hand.cpp
        tests/test_wall.cpp
        tests/test_shanten.cpp
        tests/test_decomposer.cpp
        tests/test_scorer.cpp
        tests/test_fu.cpp
        tests/test_dora.cpp
        tests/test_points.cpp
        tests/test_legal_actions.cpp
        tests/test_round.cpp
        tests/test_payments.cpp
        tests/test_game.cpp
        tests/test_yaku.cpp
        tests/test_player_view.cpp
    )
    target_link_libraries(riichi_tests PRIVATE riichi_engine Catch2::Catch2WithMain)

    include(Catch)
    catch_discover_tests(riichi_tests)
endif()
