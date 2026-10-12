cmake_minimum_required(VERSION 3.20)
project(Flagd LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(FLAGD_BUILD_TESTS "Build unit tests" ON)

# Older dependency CMakeLists declare minimums CMake 4 no longer accepts.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
  URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(raylib)

# ---- Core game logic (no raylib) -------------------------------------------
add_library(flagd_core STATIC
  src/core/board.cpp
  src/core/boardgen.cpp
  src/core/solver.cpp
  src/core/game_state.cpp
  src/core/map.cpp
  src/core/run.cpp
  src/core/meta.cpp
  src/core/content/content.cpp
)
target_include_directories(flagd_core PUBLIC src)
target_compile_options(flagd_core PRIVATE -Wall -Wextra -Wpedantic)

# ---- Game executable ---------------------------------------------------------
add_executable(flagd
  src/game/main.cpp
  src/game/app.cpp
  src/game/audio.cpp
  src/game/render/atlas.cpp
  src/game/render/ui.cpp
  src/game/scenes/title_scene.cpp
  src/game/scenes/map_scene.cpp
  src/game/scenes/board_scene.cpp
  src/game/scenes/room_scenes.cpp
  src/game/scenes/meta_scenes.cpp
)
target_link_libraries(flagd PRIVATE flagd_core raylib)
target_compile_options(flagd PRIVATE -Wall -Wextra)
if(MINGW)
  # Self-contained exe: no MSYS2 runtime DLLs needed next to it.
  target_link_options(flagd PRIVATE -static)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(flagd PROPERTIES WIN32_EXECUTABLE ON)
  endif()
endif()

# Ship only the art the game uses next to the executable.
add_custom_command(TARGET flagd POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:flagd>/assets
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/assets/1bitpack/Tilesheet/monochrome-transparent_packed.png
    $<TARGET_FILE_DIR:flagd>/assets/sheet.png)

# ---- Tests -------------------------------------------------------------------
if(FLAGD_BUILD_TESTS)
  FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(doctest)
  enable_testing()
  add_executable(flagd_tests
    tests/test_main.cpp
    tests/test_board.cpp
    tests/test_solver.cpp
    tests/test_game.cpp
    tests/test_map.cpp
    tests/test_balance.cpp
    tests/test_meta.cpp
  )
  target_link_libraries(flagd_tests PRIVATE flagd_core doctest::doctest)
  if(MINGW)
    target_link_options(flagd_tests PRIVATE -static)
  endif()
  add_test(NAME flagd_tests COMMAND flagd_tests)
endif()
