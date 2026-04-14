# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`smartmet-library-regression` is a **header-only** C++ regression test framework (`tframe.h`) used across the SmartMet Server ecosystem. It provides macros and a base class for writing test suites — it is a foundation-level dependency with no SmartMet dependencies of its own.

## Build commands

```bash
make              # No-op (header-only library, nothing to compile)
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

Every test function **must** end with one of the three macros — if a test function returns normally, the framework prints "error in test (ending macro missing?)".

## Consumers

This library is `#include`d by test suites throughout the SmartMet ecosystem (macgyver, newbase, spine, engines, plugins, etc.). Changes here affect test infrastructure across all projects.
