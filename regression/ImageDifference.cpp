#include "ImageTools.h"
#include <memory>
#include <sstream>
#include <stdexcept>
#include <Magick++.h>

using namespace Fmi;

namespace
{
    // Enchance contrast local differences
    void enhanceLocalDifferences(Magick::Image& image)
    {
        image.equalize();
    }

    double imageDifferenceImpl(
        Magick::Image& image1,
        Magick::Image& image2,
        Magick::Image *diffImage = nullptr)
    {
        // Compute PSNR between image1 and image2
        // If diffImage is not nullptr, compute the difference image as well
        if (image1.size() != image2.size() || image1.depth() != image2.depth()) {
            std::ostringstream errorMsg;
            errorMsg << "Images must be of the same size and depth for PSNR calculation:";
            errorMsg << " Got " << image1.size().width() << "x" << image1.size().height() << "x" << image1.depth();
            errorMsg << " and " << image2.size().width() << "x" << image2.size().height() << "x" << image2.depth();
            throw std::invalid_argument(errorMsg.str());
        }

        const double mse = image1.compare(image2, Magick::MetricType::MeanSquaredErrorMetric);
        const double psnr = (mse == 0)
            ? std::numeric_limits<double>::infinity()
            : 20 * log10(1.0 / sqrt(mse));

        if (diffImage) {
            // Create a difference image
            *diffImage = image1;
            diffImage->composite(image2, 0, 0, Magick::DifferenceCompositeOp);
            enhanceLocalDifferences(*diffImage);
        }

        return psnr;
    }
}

double Fmi::imageDifferencePSNR(
    const std::filesystem::path& imagePath1,
    const std::filesystem::path& imagePath2,
    std::optional<std::filesystem::path> diffImagePath)
{
    Magick::Image image1(imagePath1.string());
    Magick::Image image2(imagePath2.string());
    std::unique_ptr<Magick::Image> diffImagePtr;
    if (diffImagePath) {
        diffImagePtr = std::make_unique<Magick::Image>();
    }
    double result = imageDifferenceImpl(image1, image2, diffImagePtr.get());
    if (diffImagePtr) {
        diffImagePtr->write(diffImagePath->string());
    }
    return result;
}
