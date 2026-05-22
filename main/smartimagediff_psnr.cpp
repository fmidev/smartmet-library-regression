#include "ImageTools.h"
#include <Magick++.h>
#include <filesystem>
#include <iostream>
#include <optional>

int main(int argc, char* argv[])
{
    if (argc < 3 || argc > 4) {
        std::cerr << "Usage: " << argv[0] << " <image1> <image2> [diff_output]" << std::endl;
        return 1;
    }

    Magick::InitializeMagick(nullptr);

    try {
        std::optional<std::filesystem::path> diffPath;
        if (argc == 4) {
            diffPath = argv[3];
        }

        const double psnr = Fmi::imageDifferencePSNR(argv[1], argv[2], diffPath);
        std::cout << "PSNR: " << psnr << " dB" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
