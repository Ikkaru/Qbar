# Tests

Plain `QTest` binaries wired into CTest. No gtest, no Catch2, no mocking
framework - Qt Test ships with the Qt you already have, and these tests only
need a `Config`, a `Theme` and a `ModuleRegistry`, none of which touch a window.

Run them:

```
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Or from Qt Creator: build configuration `Release`, then the test menu.

## What is covered, and why those things

Every case below exists because the bug it guards actually happened during
development. A test with no incident behind it is not worth the maintenance.

| File | Guards |
|---|---|
| `test_config.cpp` | Re-parsing the same file must not accumulate, hotkeys must survive a round trip, unknown keys must fall back rather than throw |
| `test_theme.cpp` | The M3 generator must produce valid colours for any seed, and the fixed battery-state hues must not drift |
| `test_module_registry.cpp` | A registered id must resolve to a URL, an unregistered one must not, and layout must respect `enabled` |

## What is deliberately not covered

- **Anything that needs a window.** AppBar registration, fullscreen detection,
  the popup host's outside-click hook. These were all verified against the live
  system during development because a test double for the shell would be more
  code than the feature.
- **`GetSystemPowerStatus` and the WinRT power APIs.** They need real hardware.
  `IndicatorsService` is the one place where this hurts; see the note at the end
  of `test_config.cpp` for the workaround if you have a battery to test on.
- **Font rendering.** Whether Poppins at 11px looks as crisp as the taskbar is a
  judgement call made by looking at it, not by an assertion.
