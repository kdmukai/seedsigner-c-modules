# input_nav_harness

Deterministic keyboard-input validation for SeedSigner LVGL screens.

## Purpose

Replay scripted key sequences against real screen entrypoints and assert emitted events.

This is complementary to:
- `tools/screen_runner` (interactive manual testing)

## Build

```bash
cmake -S tests/input_nav_harness \
      -B tests/input_nav_harness/build \
      -DLVGL_ROOT=/path/to/lvgl
cmake --build tests/input_nav_harness/build -j
```

## Run

```bash
tests/input_nav_harness/build/input_nav_harness \
  tests/input_nav_harness/cases/basic_nav_cases.json
```
