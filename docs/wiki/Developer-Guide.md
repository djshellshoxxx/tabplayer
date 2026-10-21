# Developer Guide

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

- JUCE 8.0.10 is fetched by CMake `FetchContent`.
- Unit tests: `TabPlayerTests` from `tests/PlaybackEngineTests.cpp`, linked against the core sources (`TABPLAYER_CORE` in `CMakeLists.txt`).
- `TABPLAYER_BUILD_TESTS` (default ON) controls the test target.
- On Linux, the JUCE helper needs X11 development headers. A configure without them fails (seen on a machine without them; not fixed in this pass).
- CI: `.github/workflows/ci.yml`. The build and tests were not run in this documentation pass.
