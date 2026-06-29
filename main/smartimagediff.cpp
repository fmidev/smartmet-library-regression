// smartimagediff - structural, anti-aliasing-aware image regression check.
//
// Unlike smartimagediff_psnr (a global PSNR average that cannot tell a missing
// glyph from harmless cross-OS anti-aliasing noise), this judges on the
// geometry of the surviving differences: it tolerates sub-pixel / font-hinting
// jitter spread over every edge but fails on a spatially clustered change.
//
// Usage:
//   smartimagediff <result> <expected> [overlay.png]
// Exit code: 0 = OK, 1 = regression, 2 = error.
//
// Tunables (env): SID_THRESHOLD, SID_STRONG, SID_MIN_CLUSTER, SID_FAIL_CLUSTER,
//                 SID_MAX_AA_FRACTION, SID_AA_FLOOD_MIN

#include "StructuralImageDiff.h"
#include <Magick++.h>
#include <cstdlib>
#include <iostream>
#include <optional>

namespace
{
double envd(const char* k, double d)
{
    const char* v = std::getenv(k);
    return v ? std::atof(v) : d;
}
}  // namespace

int main(int argc, char* argv[])
{
    if (argc < 3 || argc > 4)
    {
        std::cerr << "Usage: " << argv[0] << " <result> <expected> [overlay.png]\n";
        return 2;
    }

    Magick::InitializeMagick(nullptr);

    try
    {
        Fmi::StructuralDiffOptions opts;
        opts.threshold = envd("SID_THRESHOLD", opts.threshold);
        opts.strong = envd("SID_STRONG", opts.strong);
        opts.minClusterArea = (long)envd("SID_MIN_CLUSTER", opts.minClusterArea);
        opts.failClusterArea = (long)envd("SID_FAIL_CLUSTER", opts.failClusterArea);
        opts.maxAaFraction = envd("SID_MAX_AA_FRACTION", opts.maxAaFraction);
        opts.aaFloodMinPixels = (long)envd("SID_AA_FLOOD_MIN", opts.aaFloodMinPixels);

        std::optional<std::filesystem::path> overlay;
        if (argc == 4)
            overlay = argv[3];

        const auto r = Fmi::structuralImageDiff(argv[1], argv[2], overlay, opts);

        if (r.sizeMismatch)
        {
            std::cout << "FAIL  size mismatch " << r.width << "x" << r.height << "\n";
            return 1;
        }

        std::cout << (r.fail ? "FAIL" : "OK") << "  largest_cluster=" << r.largestClusterArea
                  << " clusters=" << r.clusterCount << " changed_px=" << r.significantPixels
                  << " aa_ignored=" << r.aaIgnoredPixels;
        if (r.largestClusterArea > 0)
            std::cout << " at=[" << r.boxX << "," << r.boxY << " " << r.boxW << "x" << r.boxH << "]";
        if (r.aaFlood)
            std::cout << " aa_flood";
        std::cout << "\n";
        return r.fail ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }
}
