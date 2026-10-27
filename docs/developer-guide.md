# regression developer guide

This guide is for developers who change `smartmet-library-regression`, or write tests with
it. The package has two parts:

* **`tframe.h`**, the header-only unit test framework used by most SmartMet libraries
  (macgyver, newbase, locus, calculator, textgen, giza, woml, …);
* **image comparison**: `libsmartmet-regression.so` with `Fmi::imageDifferencePSNR()`,
  the structural image diff and an animated WebP helper, and the programs
  `smartimagediff_psnr`, `smartimagediff` and `smartpngdiff`, used by the image tests of the
  WMS plugin and the drawing tools.

[structural-image-diff.md](structural-image-diff.md) describes the structural diff in
detail, and the manual pages in `docs/man/` describe the programs.

## Contents

1. [Building and testing](#1-building-and-testing)
2. [Writing tframe tests](#2-writing-tframe-tests)
3. [Image comparison](#3-image-comparison)
4. [Compatibility](#4-compatibility)
5. [Known pitfalls](#5-known-pitfalls)

---

## 1. Building and testing

```bash
make               # libsmartmet-regression.so and the image programs (needs Magick++)
make test
make headertest    # each header compiles on its own
```

## 2. Writing tframe tests

```cpp
#include <regression/tframe.h>

namespace Tests
{
void parse_iso()
{
  if (Fmi::TimeParser::parse("2026-09-25T12:00:00Z") != expected)
    TEST_FAILED("ISO time parsing failed");
  TEST_PASSED();
}

class tests : public tframe::tests
{
  const char* error_message_prefix() const override { return "\n\t"; }
  void test() override { TEST(parse_iso); }
};
}  // namespace Tests

int main()
{
  Tests::tests t;
  return t.run();
}
```

* A test function ends by **throwing**: `TEST_PASSED()`, `TEST_FAILED(message)` or
  `TEST_NOT_IMPLEMENTED()`. Code after them does not run.
* `TEST(name)` in `test()` runs one function and prints one line: `passed`, `failed` with
  the message, file and line, `not implemented`, or `unexpected exception` with the
  exception's `what()` (for `Fmi::Exception`, the innermost message).
* `run()` prints the totals and returns `EXIT_FAILURE` if any test failed or returned
  without an ending macro, otherwise `EXIT_SUCCESS`. Not-implemented tests do not count
  as failures.
* There is no command-line selection of tests: all registered tests run in order.
* Output goes to `std::cout` by default; the constructor overloads take another stream,
  the fill character and the column width, and the virtual `passed()`, `failed()`, … methods
  can be overridden to change the reporting.

## 3. Image comparison

| Tool / function | Measure |
|-----------------|---------|
| `smartimagediff_psnr a b [diff]`, `Fmi::imageDifferencePSNR()` | Global PSNR in dB; optionally writes a difference image. The callers decide the thresholds (the WMS tests use ≥ 50 OK, ≥ 20 warning). |
| `smartimagediff a b [overlay]` | Structural diff: ignores antialiasing and font jitter, and fails on connected clusters of changed pixels. Exit code 0 OK, 1 regression, 2 error; tunables via `SID_*` environment variables. |
| `smartpngdiff ok new diff` | Shell script: the PSNR verdict of `smartimagediff_psnr` (≥ 50 dB OK, ≥ 20 dB warning, exit 100 on failure), with the `smartimagediff` verdict appended in shadow mode and its overlay as the diff image. |
| `Fmi::CreateAnimatedWebP()` | Builds an animated WebP from PNG frames, for inspecting animation test results. |

## 4. Compatibility

`tframe.h` is included by the test programs of many repositories, so any change in its
macros or output format changes every test suite's output. Keep the output format stable,
since some test scripts parse it. The library and programs are separate from the header:
the header does not need the library.

## 5. Known pitfalls

* **A test without an ending macro fails the run.** If a test function returns normally,
  tframe prints "error in test (ending macro missing?)" and `run()` returns
  `EXIT_FAILURE`. End every path of a test function with a macro.
* **The "ending macro missing" message goes to `std::cout`**, even when the tests were
  given another output stream.
* **PSNR hides small changes**: a missing glyph barely moves the average. Use
  `smartimagediff` where small but meaningful changes matter (see
  [structural-image-diff.md](structural-image-diff.md)).
