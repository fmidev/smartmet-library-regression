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

// A low-amplitude change spread over the whole frame (uniform brightness shift)
// must NOT fail: it touches every pixel but no pixel crosses the "significant"
// amplitude bar, so it is filtered before clustering.
void test_low_amplitude_ignored()
{
    auto base = writeScene("lo_base.png");
    Magick::Image img(base.string());
    const std::size_t w = img.columns(), h = img.rows();
    std::vector<uint8_t> buf(w * h * 4);
    img.write(0, 0, w, h, "RGBA", Magick::CharPixel, buf.data());
    for (std::size_t i = 0; i < w * h; ++i)
        for (int c = 0; c < 3; ++c)
        {
            const int v = buf[i * 4 + c] + 40;  // crosses base threshold, stays under "strong"
            buf[i * 4 + c] = static_cast<uint8_t>(v > 255 ? 255 : v);
        }
    Magick::Image shifted(w, h, "RGBA", Magick::CharPixel, buf.data());
    auto sp = P("lo_shift.png");
    shifted.write(sp.string());

    auto r = Fmi::structuralImageDiff(sp, base);
    if (r.candidatePixels == 0)
        TEST_FAILED("brightness shift produced no candidate pixels (fixture too weak)");
    if (r.significantPixels != 0)
        TEST_FAILED("low-amplitude shift produced significant pixels: " +
                    std::to_string(r.significantPixels));
    if (r.fail)
        TEST_FAILED("uniform low-amplitude shift was flagged as a regression");
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
        TEST(test_solid_block_caught);
        TEST(test_cluster_boundary);
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
