# smartmet-library-regression

Part of [SmartMet Server](https://github.com/fmidev/smartmet-server). See the [SmartMet Server documentation](https://github.com/fmidev/smartmet-server) for an overview of the ecosystem.

## Overview

The regression library provides the unit test framework and the image comparison tools used across SmartMet Server tools and libraries.

## Features

- `tframe.h`: a header-only unit test framework (`TEST`, `TEST_PASSED`, `TEST_FAILED`, `TEST_NOT_IMPLEMENTED`)
- `smartimagediff_psnr`: global PSNR comparison of two images, with an optional difference image
- `smartimagediff`: structural image comparison that tolerates anti-aliasing and font jitter, with an overlay of the changes
- `smartpngdiff`: ImageMagick-based PNG comparison script
- `libsmartmet-regression.so`: the comparison functions and an animated WebP helper for inspecting test results

## Usage

Used by the test suites of [smartmet-qdtools](https://github.com/fmidev/smartmet-qdtools), [smartmet-qdcontour](https://github.com/fmidev/smartmet-qdcontour), and other SmartMet components.

## Documentation

- [Developer guide](docs/developer-guide.md): writing tframe tests, image comparison tools, pitfalls

## License

MIT — see [LICENSE](LICENSE)

## Contributing

Bug reports and pull requests are welcome on [GitHub](../../issues).
