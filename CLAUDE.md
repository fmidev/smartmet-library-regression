# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Full developer documentation: `docs/developer-guide.md`.

`smartmet-library-regression` provides the header-only C++ test framework `tframe.h` used across the SmartMet Server ecosystem, and image comparison tools: `libsmartmet-regression.so` (`Fmi::imageDifferencePSNR`, the structural image diff, `Fmi::CreateAnimatedWebP`) and the programs `smartimagediff_psnr`, `smartimagediff` and `smartpngdiff`. The library and programs need Magick++; `tframe.h` itself has no dependencies.

## Build commands

```bash
make              # Build libsmartmet-regression.so and the image comparison programs
make test         # Build and run tests in test/
make install      # Install headers to $(includedir)/smartmet/regression/
make rpm          # Build RPM package
make headertest   # Verify each header is self-sufficient
make clean        # Clean test artifacts
```

### Running a single test

```bash
cd test && make TestRegression && ./TestRegression
```

The test Makefile compiles `TestRegression.cpp` directly with `$(CXX) -I.. -Wall` — no smartbuildcfg dependency needed for tests.

## How the framework works

Tests are written by:
1. Defining free functions that use `TEST_PASSED()`, `TEST_FAILED("msg")`, or `TEST_NOT_IMPLEMENTED()` macros (these throw exception types caught by the harness)
2. Subclassing `tframe::tests` and overriding `test()` to list test functions via the `TEST(name)` macro
3. Calling `.run()` which returns `EXIT_SUCCESS` if all tests pass (unimplemented tests don't count as failures), `EXIT_FAILURE` otherwise

Every test function **must** end with one of the three macros — if a test function returns normally, the framework prints "error in test (ending macro missing?)" (always to `std::cout`), the summary says "failed", and `run()` returns `EXIT_FAILURE`.

## Consumers

This library is `#include`d by test suites throughout the SmartMet ecosystem (macgyver, newbase, spine, engines, plugins, etc.). Changes here affect test infrastructure across all projects.
