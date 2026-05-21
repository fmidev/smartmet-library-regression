#pragma once

#include <string>
#include <optional>
#include <vector>
#include <filesystem>

namespace Fmi
{

double imageDifferencePSNR(
    const std::filesystem::path& imagePath1,
    const std::filesystem::path& imagePath2,
    std::optional<std::filesystem::path> diffImagePath = std::nullopt);

/**
 * Create an animated WebP image from a sequence of PNG images.
 *
 * @param png_paths List of input PNG file paths.
 * @param output_webp Output WebP file path.
 * @param delay_ms Delay between frames in milliseconds (default: 100).
 * @param loop_count Number of times to loop the animation (0 = infinite, default: 0).
 * @param lossless Whether to use lossless encoding (default: true).
 * @param quality Quality level for lossy encoding (0-100, default: 90).
 * @param method WebP encoder effort (0-6, default: 6).
 */
void CreateAnimatedWebP(
    const std::vector<std::string>& png_paths,
    const std::string& output_webp,
    unsigned delay_ms   = 100,
    unsigned loop_count = 0,
    bool lossless   = true,
    int quality    = 90,
    int method     = 6);

} // namespace Fmi
