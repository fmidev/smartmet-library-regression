// Unit tests for Fmi::structuralImageDiff.
//
// The fixtures are generated synthetically with Magick++ at runtime — no
// committed binary PNGs. Each case models one property of the algorithm rather
// than reproducing one captured image pair, so a reviewer can see *why* a case
// should pass or fail, and the test stays reproducible.
//
// The two cases at the heart of the tool are test_antialiasing_ignored (a
// harmless cross-OS-style edge softening, modelled by a slight blur, must NOT
// fail) and test_solid_block_caught (a spatially clustered change must fail).
// Those are the synthetic stand-ins for the RHEL8 -> Rocky10 anti-aliasing
// differences that motivated the tool, expressed as properties the algorithm
// must hold instead of one sampled image pair.

#include <regression/StructuralImageDiff.h>
#include <regression/tframe.h>

#include <Magick++.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
fs::path g_tmp;

fs::path P(const std::string& name)
{
    return g_tmp / name;
}

// An anti-aliased base scene: gray background with a filled circle and a
// diagonal stroke (naturally anti-aliased curved/slanted edges, which the
// blur test softens) plus a clear flat region around (130,130)-(160,160) for
// block placement.
Magick::Image baseScene()
{
    Magick::Image img(Magick::Geometry(200, 200), Magick::Color("gray50"));
    img.strokeAntiAlias(true);
    // Image-level styling + single-Drawable draw() calls: the batch draw()
    // overload takes std::list on ImageMagick 6 and std::vector on 7 with no
    // common type, while draw(const Drawable&) works on both.
    img.fillColor(Magick::Color("black"));
    img.strokeColor(Magick::Color("black"));
    img.draw(Magick::DrawableCircle(55, 55, 55, 25));  // center (55,55), r=30
    img.strokeWidth(2);
    img.draw(Magick::DrawableLine(10, 150, 150, 10));  // diagonal stroke
    img.magick("PNG");
    return img;
}

fs::path writeScene(const std::string& name)
{
    auto p = P(name);
    auto img = baseScene();
    img.write(p.string());
    return p;
}

// Draw a solid contrasting block onto the base scene and save it.
fs::path sceneWithBlock(const std::string& name, int x1, int y1, int x2, int y2)
{
    auto base = baseScene();
    base.fillColor(Magick::Color("red"));
    base.strokeColor(Magick::Color("red"));
    base.draw(Magick::DrawableRectangle(x1, y1, x2, y2));
    auto p = P(name);
    base.write(p.string());
    return p;
}

}  // namespace

// ---------------------------------------------------------------------------

void test_identical()
{
    auto base = writeScene("id_base.png");
    auto r = Fmi::structuralImageDiff(base, base);
    if (r.sizeMismatch)
        TEST_FAILED("identical images reported a size mismatch");
    if (r.fail)
        TEST_FAILED("identical images were flagged as a regression");
    if (r.clusterCount != 0)
        TEST_FAILED("identical images produced clusters: " + std::to_string(r.clusterCount));
    TEST_PASSED();
}

void test_size_mismatch()
{
    Magick::Image a(Magick::Geometry(200, 200), Magick::Color("gray50"));
    Magick::Image b(Magick::Geometry(150, 200), Magick::Color("gray50"));
    auto pa = P("sz_a.png"), pb = P("sz_b.png");
    a.write(pa.string());
    b.write(pb.string());
    auto r = Fmi::structuralImageDiff(pa, pb);
    if (!r.sizeMismatch)
        TEST_FAILED("differing dimensions were not reported as a size mismatch");
    if (!r.fail)
        TEST_FAILED("a size mismatch did not fail the comparison");
    TEST_PASSED();
}

// Harmless cross-OS-style edge softening (a slight blur) must NOT fail: the
// per-edge differences are thin specks, never a solid cluster.
void test_antialiasing_ignored()
{
    auto base = writeScene("aa_base.png");
    Magick::Image blurred(base.string());
    blurred.blur(0.0, 0.6);
    auto bp = P("aa_blur.png");
    blurred.write(bp.string());

    auto r = Fmi::structuralImageDiff(bp, base);
    // Prove the case is meaningful: the blur must produce real candidate
    // differences that the AA detector then discards, not just pixels that fall
    // below the base threshold (which would pass for the wrong reason).
    if (r.candidatePixels == 0)
        TEST_FAILED("blur produced no candidate pixels (fixture too weak)");
    if (r.aaIgnoredPixels == 0)
        TEST_FAILED("blur differences were not exercised through the anti-aliasing path");
    if (r.fail)
        TEST_FAILED("anti-aliasing-class blur was flagged as a regression (largest_cluster=" +
                    std::to_string(r.largestClusterArea) + ")");
    TEST_PASSED();
}

// Shift every colour channel of the base scene by the given amount
fs::path shiftedScene(const std::string& baseName, const std::string& name, int shift)
{
    auto base = writeScene(baseName);
    Magick::Image img(base.string());
    const std::size_t w = img.columns(), h = img.rows();
    std::vector<uint8_t> buf(w * h * 4);
    img.write(0, 0, w, h, "RGBA", Magick::CharPixel, buf.data());
    for (std::size_t i = 0; i < w * h; ++i)
        for (int c = 0; c < 3; ++c)
        {
            const int v = buf[i * 4 + c] + shift;
            buf[i * 4 + c] = static_cast<uint8_t>(v > 255 ? 255 : v);
        }
    Magick::Image shifted(w, h, "RGBA", Magick::CharPixel, buf.data());
    auto sp = P(name);
    shifted.write(sp.string());
    return sp;
}

// A tiny change spread over the whole frame (a uniform shift of a few levels,
// as from palette quantization) must NOT fail: no pixel crosses the area
// change threshold, let alone the significant amplitude bar.
void test_low_amplitude_ignored()
{
    auto sp = shiftedScene("lo_base.png", "lo_shift.png", 3);
    auto r = Fmi::structuralImageDiff(sp, P("lo_base.png"));
    if (r.significantPixels != 0)
        TEST_FAILED("low-amplitude shift produced significant pixels: " +
                    std::to_string(r.significantPixels));
    if (r.areaPixels != 0)
        TEST_FAILED("low-amplitude shift produced area change pixels: " +
                    std::to_string(r.areaPixels));
    if (r.fail)
        TEST_FAILED("uniform low-amplitude shift was flagged as a regression");
    TEST_PASSED();
}

// A clearly visible brightness change of the whole frame must fail even though
// no pixel crosses the significant amplitude bar: it is one large area change.
void test_uniform_shift_caught()
{
    auto sp = shiftedScene("us_base.png", "us_shift.png", 40);
    auto r = Fmi::structuralImageDiff(sp, P("us_base.png"));
    if (r.significantPixels != 0)
        TEST_FAILED("brightness shift unexpectedly produced significant pixels (fixture too strong)");
    if (!r.areaChange)
        TEST_FAILED("uniform brightness shift was not reported as an area change (largest_area=" +
                    std::to_string(r.largestArea) + ")");
    if (!r.fail)
        TEST_FAILED("uniform brightness shift was not caught");
    TEST_PASSED();
}

// A map with pale land and sea colours (meteorological light background)
fs::path paleMap(const std::string& name, const std::string& land, const std::string& sea, int x2)
{
    Magick::Image img(Magick::Geometry(300, 200), Magick::Color(sea));
    img.strokeAntiAlias(true);
    img.fillColor(Magick::Color(land));
    img.strokeColor(Magick::Color(land));
    img.draw(Magick::DrawableRectangle(20, 20, x2, 180));
    img.strokeColor(Magick::Color("gray60"));
    img.strokeWidth(1);
    img.draw(Magick::DrawableLine(0, 100, 299, 100));  // a road, unchanged
    auto p = P(name);
    img.write(p.string());
    return p;
}

// Swapping pale land and sea colours over a large area (difference 0.085,
// below the base threshold) must fail. The cluster check sees nothing.
void test_pale_recolour_caught()
{
    auto base = paleMap("pale_base.png", "#f0efeb", "#cfdfe8", 200);
    auto swapped = paleMap("pale_swap.png", "#cfdfe8", "#f0efeb", 200);
    auto r = Fmi::structuralImageDiff(swapped, base);
    if (r.candidatePixels != 0)
        TEST_FAILED("pale colour swap crossed the base threshold (fixture too strong)");
    if (!r.areaChange || !r.fail)
        TEST_FAILED("pale land/sea colour swap was not caught (largest_area=" +
                    std::to_string(r.largestArea) + ")");
    if (r.areaX > 30 || r.areaY > 30)
        TEST_FAILED("area bounding box missed the change: " + std::to_string(r.areaX) + "," +
                    std::to_string(r.areaY));
    TEST_PASSED();
}

// The area check is driven by the size of the largest connected area: a pale
// coastline moved by 1px is a thin line and passes, land that grew by a block
// of 0.9% of the image fails.
void test_area_boundary()
{
    {
        auto base = paleMap("ab1_base.png", "#f0efeb", "#cfdfe8", 200);
        auto mod = paleMap("ab1_mod.png", "#f0efeb", "#cfdfe8", 201);  // 1px x 161px strip
        auto r = Fmi::structuralImageDiff(mod, base);
        if (r.areaChange || r.fail)
            TEST_FAILED("a 1px coastline displacement was wrongly flagged (largest_area=" +
                        std::to_string(r.largestArea) + ")");
    }
    {
        auto base = paleMap("ab2_base.png", "#f0efeb", "#cfdfe8", 200);
        auto mod = paleMap("ab2_mod.png", "#f0efeb", "#cfdfe8", 220);  // 20 x 161 = 1.07% of 300x200
        auto r = Fmi::structuralImageDiff(mod, base);
        if (!r.areaChange || !r.fail)
            TEST_FAILED("a pale area change of ~1% of the image was not caught (largest_area=" +
                        std::to_string(r.largestArea) + ")");
    }
    TEST_PASSED();
}

// A solid, spatially clustered change (a dropped/added feature) must fail, and
// the reported bounding box must land on the change.
void test_solid_block_caught()
{
    auto base = writeScene("blk_base.png");
    auto mod = sceneWithBlock("blk_mod.png", 130, 130, 160, 160);  // 31x31 solid
    auto r = Fmi::structuralImageDiff(mod, base);
    if (!r.fail)
        TEST_FAILED("a solid block change was not caught");
    if (r.largestClusterArea < 400)
        TEST_FAILED("block cluster unexpectedly small: " + std::to_string(r.largestClusterArea));
    if (r.boxX < 120 || r.boxX > 140 || r.boxY < 120 || r.boxY > 140)
        TEST_FAILED("block bounding box missed the change: " + std::to_string(r.boxX) + "," +
                    std::to_string(r.boxY));
    TEST_PASSED();
}

// The fail decision is driven by cluster area: a block below failClusterArea
// passes, one above fails. This pins the threshold as the actual boundary.
void test_cluster_boundary()
{
    {
        auto base = writeScene("cb1_base.png");
        auto mod = sceneWithBlock("cb1_mod.png", 130, 130, 136, 136);  // 7x7 = 49 < 80
        auto r = Fmi::structuralImageDiff(mod, base);
        if (r.fail)
            TEST_FAILED("sub-threshold cluster (~49 px) was wrongly flagged");
    }
    {
        auto base = writeScene("cb2_base.png");
        auto mod = sceneWithBlock("cb2_mod.png", 130, 130, 141, 141);  // 12x12 = 144 > 80
        auto r = Fmi::structuralImageDiff(mod, base);
        if (!r.fail)
            TEST_FAILED("above-threshold cluster (~144 px) was not flagged");
    }
    TEST_PASSED();
}

// A sub-pixel geometry displacement (a thin line that moved by ~1px along its
// whole length) reads pixel-by-pixel as anti-aliasing -- every displaced pixel
// is edge-adjacent -- so it forms no significant cluster, yet it floods the
// AA-ignored count across a large share of the frame. The AA-flood guard must
// catch it. This is the synthetic stand-in for the map-simplification false
// negative (a simplified coastline taking a slightly different path) that the
// cluster-only verdict silently passed.
void test_aa_flood_caught()
{
    auto lines = [](const std::string& name, int dx) {
        Magick::Image img(Magick::Geometry(400, 400), Magick::Color("white"));
        img.strokeAntiAlias(true);
        img.strokeColor(Magick::Color("black"));
        img.strokeWidth(1);
        for (int i = -400; i < 400; i += 12)
            img.draw(Magick::DrawableLine(i + dx, 0, i + dx + 400, 399));  // many diagonals
        auto p = P(name);
        img.write(p.string());
        return p;
    };
    auto base = lines("flood_base.png", 0);
    auto shifted = lines("flood_shift.png", 1);  // whole field displaced by 1px

    auto r = Fmi::structuralImageDiff(shifted, base);
    if (r.aaIgnoredPixels == 0)
        TEST_FAILED("shifted line field produced no anti-aliasing pixels (fixture too weak)");
    if (!r.aaFlood)
        TEST_FAILED("pervasive 1px line displacement did not trip the AA-flood guard (aa_ignored=" +
                    std::to_string(r.aaIgnoredPixels) + ")");
    if (!r.fail)
        TEST_FAILED("AA flood did not fail the comparison");
    TEST_PASSED();
}

// The benign anti-aliasing case (a localized blur) must NOT trip the flood
// guard: too few AA pixels in absolute terms, even though their fraction of the
// small fixture is not tiny. Pins the absolute-floor half of the guard.
void test_aa_flood_not_tripped_by_blur()
{
    auto base = writeScene("flood_aa_base.png");
    Magick::Image blurred(base.string());
    blurred.blur(0.0, 0.6);
    auto bp = P("flood_aa_blur.png");
    blurred.write(bp.string());

    auto r = Fmi::structuralImageDiff(bp, base);
    if (r.aaFlood)
        TEST_FAILED("localized blur wrongly tripped the AA-flood guard (aa_ignored=" +
                    std::to_string(r.aaIgnoredPixels) + ")");
    if (r.fail)
        TEST_FAILED("localized blur was flagged as a regression");
    TEST_PASSED();
}

// ---------------------------------------------------------------------------

class StructuralImageDiffTests : public tframe::tests
{
   public:
    StructuralImageDiffTests() : tframe::tests('.', 50) {}

    void test() override
    {
        TEST(test_identical);
        TEST(test_size_mismatch);
        TEST(test_antialiasing_ignored);
        TEST(test_low_amplitude_ignored);
        TEST(test_uniform_shift_caught);
        TEST(test_pale_recolour_caught);
        TEST(test_area_boundary);
        TEST(test_solid_block_caught);
        TEST(test_cluster_boundary);
        TEST(test_aa_flood_caught);
        TEST(test_aa_flood_not_tripped_by_blur);
    }
};

int main()
{
    Magick::InitializeMagick(nullptr);

    g_tmp = fs::temp_directory_path() / "smartimagediff_test";
    fs::create_directories(g_tmp);

    std::cout << "\n### Testing Fmi::structuralImageDiff\n";
    StructuralImageDiffTests t;
    const int rc = t.run();

    std::error_code ec;
    fs::remove_all(g_tmp, ec);
    return rc;
}
