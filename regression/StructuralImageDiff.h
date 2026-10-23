#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>

namespace Fmi
{
// Structural, anti-aliasing-aware image comparison.
//
// Motivation: a global metric (PSNR / mean squared error) cannot separate
// "sub-pixel anti-aliasing and font-hinting jitter spread thinly over every
// edge" (which should be tolerated, e.g. when the accepted image was rendered
// on a different OS / freetype version) from "a spatially clustered solid
// change such as a missing glyph or a dropped map feature" (which must be
// caught). The two can produce the *same* average error.
//
// This compares per pixel with the pixelmatch YIQ perceptual color metric
// (Yee), discards anti-aliasing-class pixels (Vyšniauskas detector), keeps
// only high-amplitude differences, clusters them with 8-connectivity, and
// judges on cluster *geometry*: a real change forms at least one solid
// cluster, whereas rendering jitter only ever produces thin per-edge specks.

struct StructuralDiffOptions
{
    double threshold = 0.1;     // base per-pixel YIQ sensitivity (pixelmatch)
    double strong = 0.25;       // normalized amplitude for a "significant" pixel
    long minClusterArea = 8;    // clusters smaller than this are counted as noise
    long failClusterArea = 80;  // any surviving cluster >= this -> regression

    // Anti-aliasing flood guard. The per-pixel AA detector cannot tell a
    // *stationary* softened edge (harmless cross-OS / font-hinting jitter) from
    // a thin line that *moved* by ~1px along its whole length (a real
    // sub-pixel geometry change, e.g. a simplified coastline) -- both look like
    // edge-adjacent differences, so the latter is silently absorbed into the
    // AA-ignored count. Genuine AA stays a thin fringe (<=0.1% of the frame
    // across the WMS suite); a displacement floods a far larger share. When the
    // AA-classified pixels exceed BOTH an absolute floor and a fraction of the
    // image, treat it as a regression instead of passing. Requiring both avoids
    // false positives on small images (high fraction but few pixels, e.g. a
    // blurred 200x200 fixture) and on huge sparse ones (many pixels, tiny
    // fraction). Set maxAaFraction <= 0 to disable.
    double maxAaFraction = 0.003;  // 0.3% of the image area
    long aaFloodMinPixels = 1000;  // absolute floor below which the fraction is not trusted

    // Area change guard. Pale palettes differ by less than the base threshold
    // (meteorological land vs sea 0.085, adjacent light isoband colours 0.099),
    // so a recoloured isoband or a swapped background never becomes a candidate
    // pixel, let alone a strong one. Such a change is however a large connected
    // area, whereas rendering jitter stays a thin fringe (at most 0.15% of the
    // frame across the WMS suite). Pixels differing by more than areaThreshold
    // that are not anti-aliasing are clustered separately, and a cluster of at
    // least maxAreaFraction of the image (and areaMinPixels) is a regression.
    // areaThreshold stays above uniform palette quantization shifts (an 8 level
    // shift is ~0.03). The pixels are eroded by areaErosion pixels before
    // clustering, so that only solid areas count: softened or displaced edges
    // and thin lines vanish, a recoloured area only loses its rim.
    // Set maxAreaFraction <= 0 to disable.
    double areaThreshold = 0.03;    // normalized amplitude for an area change pixel
    int areaErosion = 2;            // erosion radius in pixels
    double maxAreaFraction = 0.002;  // 0.2% of the image area
    long areaMinPixels = 200;        // absolute floor for small images
};

struct StructuralDiffResult
{
    bool sizeMismatch = false;       // images differ in dimensions -> always a fail
    std::size_t width = 0;
    std::size_t height = 0;
    long candidatePixels = 0;        // pixels exceeding the base threshold
    long aaIgnoredPixels = 0;        // candidates discarded as anti-aliasing
    long significantPixels = 0;      // pixels in clusters >= minClusterArea
    long clusterCount = 0;           // number of such clusters
    long largestClusterArea = 0;     // area of the biggest cluster
    int boxX = 0, boxY = 0, boxW = 0, boxH = 0;  // bbox of the biggest cluster
    bool aaFlood = false;            // AA-ignored pixels flooded the frame (see options)
    long areaPixels = 0;             // non-AA pixels differing by more than areaThreshold
    long largestArea = 0;            // area of the biggest connected cluster of them
    int areaX = 0, areaY = 0, areaW = 0, areaH = 0;  // bbox of the biggest area cluster
    bool areaChange = false;         // the biggest area cluster is a regression (see options)
    bool fail = false;               // verdict
};

// Compare image1 (result) against image2 (expected). If overlayPath is given,
// write a human-facing diff: the expected image faded to faint grayscale with
// the significant changes painted red and boxed, so the eye lands directly on
// the regression instead of scanning an equalized full-frame of noise.
StructuralDiffResult structuralImageDiff(const std::filesystem::path& image1,
                                         const std::filesystem::path& image2,
                                         std::optional<std::filesystem::path> overlayPath = std::nullopt,
                                         const StructuralDiffOptions& opts = {});

}  // namespace Fmi
