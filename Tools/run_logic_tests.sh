#!/usr/bin/env sh
# Build and run the pure C++ logic tests without Unreal (needs CMake 3.20+ and a C++20 compiler).
#   Tools/run_logic_tests.sh              all tests, normal + unity/UE-macro build
#   Tools/run_logic_tests.sh --validate   print every data issue in Data/Narrative
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cmake -S "$ROOT/Tools/LogicTests" -B "$ROOT/Build/LogicTests" -DCMAKE_BUILD_TYPE=Debug >/dev/null
cmake --build "$ROOT/Build/LogicTests" --parallel
if [ "$1" = "--validate" ]; then
  exec "$ROOT/Build/LogicTests/gon_logic_tests" --validate
fi
"$ROOT/Build/LogicTests/gon_logic_tests" "$@"
"$ROOT/Build/LogicTests/gon_logic_tests_unity" "$@" | tail -n 1
