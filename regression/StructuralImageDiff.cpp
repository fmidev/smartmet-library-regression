#include "StructuralImageDiff.h"

#include <Magick++.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <vector>

namespace
{
// ---- pixelmatch color delta (port of mapbox/pixelmatch) --------------------

inline double rgb2y(double r, double g, double b)
{
  return r * 0.29889531 + g * 0.58662247 + b * 0.11448223;
}
inline double rgb2i(double r, double g, double b)
{
  return r * 0.59597799 - g * 0.27417610 - b * 0.32180189;
}
inline double rgb2q(double r, double g, double b)
{
  return r * 0.21147017 - g * 0.52261711 + b * 0.31114694;
}
inline double blendWhite(double c, double a)
{
  return 255.0 + (c - 255.0) * a;
}

// Signed squared YIQ color delta. With onlyY == true, returns the signed
// brightness difference (used by the anti-aliasing detector).
double colorDelta(
    const uint8_t* a, const uint8_t* b, std::size_t i, std::size_t j, bool onlyY = false)
{
  double r1 = a[i];
  double g1 = a[i + 1];
  double b1 = a[i + 2];
  double a1 = a[i + 3];
  double r2 = b[j];
  double g2 = b[j + 1];
  double b2 = b[j + 2];
  double a2 = b[j + 3];
  if (a1 == a2 && r1 == r2 && g1 == g2 && b1 == b2)
    return 0;
  if (a1 < 255)
  {
    a1 /= 255;
    r1 = blendWhite(r1, a1);
    g1 = blendWhite(g1, a1);
    b1 = blendWhite(b1, a1);
  }
  if (a2 < 255)
  {
    a2 /= 255;
    r2 = blendWhite(r2, a2);
    g2 = blendWhite(g2, a2);
    b2 = blendWhite(b2, a2);
  }
  const double y1 = rgb2y(r1, g1, b1);
  const double y2 = rgb2y(r2, g2, b2);
  const double y = y1 - y2;
  if (onlyY)
    return y;
  const double di = rgb2i(r1, g1, b1) - rgb2i(r2, g2, b2);
  const double dq = rgb2q(r1, g1, b1) - rgb2q(r2, g2, b2);
  const double delta = 0.5053 * y * y + 0.299 * di * di + 0.1957 * dq * dq;
  return (y1 > y2) ? -delta : delta;
}

bool hasManySiblings(const uint8_t* img, int x1, int y1, int w, int h)
{
  const int x0 = std::max(x1 - 1, 0);
  const int y0 = std::max(y1 - 1, 0);
  const int x2 = std::min(x1 + 1, w - 1);
  const int y2 = std::min(y1 + 1, h - 1);
  const std::size_t pos = (std::size_t(y1) * w + x1) * 4;
  int zeroes = (x1 == x0 || x1 == x2 || y1 == y0 || y1 == y2) ? 1 : 0;
  for (int x = x0; x <= x2; ++x)
    for (int y = y0; y <= y2; ++y)
    {
      if (x == x1 && y == y1)
        continue;
      const std::size_t p2 = (std::size_t(y) * w + x) * 4;
      if (img[pos] == img[p2] && img[pos + 1] == img[p2 + 1] && img[pos + 2] == img[p2 + 2] &&
          img[pos + 3] == img[p2 + 3])
      {
        if (++zeroes > 2)
          return true;
      }
    }
  return false;
}

// True if pixel (x1,y1) looks like anti-aliasing in `img`, cross-checked
// against `other`.
bool antialiased(const uint8_t* img, int x1, int y1, int w, int h, const uint8_t* other)
{
  const int x0 = std::max(x1 - 1, 0);
  const int y0 = std::max(y1 - 1, 0);
  const int x2 = std::min(x1 + 1, w - 1);
  const int y2 = std::min(y1 + 1, h - 1);
  const std::size_t pos = (std::size_t(y1) * w + x1) * 4;
  int zeroes = (x1 == x0 || x1 == x2 || y1 == y0 || y1 == y2) ? 1 : 0;
  double mn = 0;
  double mx = 0;
  int minX = -1;
  int minY = -1;
  int maxX = -1;
  int maxY = -1;
  for (int x = x0; x <= x2; ++x)
    for (int y = y0; y <= y2; ++y)
    {
      if (x == x1 && y == y1)
        continue;
      const double delta = colorDelta(img, img, pos, (std::size_t(y) * w + x) * 4, true);
      if (delta == 0)
      {
        if (++zeroes > 2)
          return false;
      }
      else if (delta < mn)
      {
        mn = delta;
        minX = x;
        minY = y;
      }
      else if (delta > mx)
      {
        mx = delta;
        maxX = x;
        maxY = y;
      }
    }
  if (mn == 0 || mx == 0)
    return false;
  return (hasManySiblings(img, minX, minY, w, h) && hasManySiblings(other, minX, minY, w, h)) ||
         (hasManySiblings(img, maxX, maxY, w, h) && hasManySiblings(other, maxX, maxY, w, h));
}

struct Cluster
{
  int minx, miny, maxx, maxy;
  long area;
};

}  // namespace

namespace Fmi
{
namespace
{
// Classify the pixels: 0 = same, 1 = weak/AA difference, 2 = significant difference.
// Also marks the non-AA pixels exceeding the area change threshold in areaMask.
std::vector<uint8_t> classifyPixels(const std::vector<uint8_t>& a,
                                    const std::vector<uint8_t>& b,
                                    std::size_t w,
                                    std::size_t h,
                                    const StructuralDiffOptions& opts,
                                    std::vector<uint8_t>& areaMask,
                                    StructuralDiffResult& res)
{
  const double baseSq = opts.threshold * opts.threshold;
  const double areaSq = opts.areaThreshold * opts.areaThreshold;
  const double minSq = std::min(baseSq, areaSq);

  std::vector<uint8_t> mask(w * h, 0);
  areaMask.assign(w * h, 0);
  for (int y = 0; y < (int)h; ++y)
    for (int x = 0; x < (int)w; ++x)
    {
      const std::size_t p = (std::size_t(y) * w + x) * 4;
      const double nd = std::fabs(colorDelta(a.data(), b.data(), p, p, false)) / 35215.0;
      if (nd <= minSq)
        continue;
      const bool isAA = antialiased(a.data(), x, y, w, h, b.data()) ||
                        antialiased(b.data(), x, y, w, h, a.data());
      if (nd > areaSq && !isAA)
      {
        ++res.areaPixels;
        areaMask[std::size_t(y) * w + x] = 2;
      }
      if (nd <= baseSq)
        continue;
      ++res.candidatePixels;
      const bool isStrong = std::sqrt(nd) >= opts.strong;
      if (isAA)
        ++res.aaIgnoredPixels;
      mask[std::size_t(y) * w + x] = (isStrong && !isAA) ? 2 : 1;
    }
  return mask;
}

// Flood fill the 8-connected cluster of significant pixels starting from (x,y)
Cluster fillCluster(const std::vector<uint8_t>& mask,
                    std::vector<int>& label,
                    std::size_t w,
                    std::size_t h,
                    int x,
                    int y,
                    int clusterLabel)
{
  Cluster c{x, y, x, y, 0};
  std::queue<std::pair<int, int>> q;
  q.emplace(x, y);
  label[std::size_t(y) * w + x] = clusterLabel;
  while (!q.empty())
  {
    const auto [cx, cy] = q.front();
    q.pop();
    ++c.area;
    c.minx = std::min(c.minx, cx);
    c.maxx = std::max(c.maxx, cx);
    c.miny = std::min(c.miny, cy);
    c.maxy = std::max(c.maxy, cy);
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx)
      {
        const int nx = cx + dx;
        const int ny = cy + dy;
        if (nx < 0 || ny < 0 || nx >= (int)w || ny >= (int)h)
          continue;
        const std::size_t ni = std::size_t(ny) * w + nx;
        if (mask[ni] == 2 && !label[ni])
        {
          label[ni] = clusterLabel;
          q.emplace(nx, ny);
        }
      }
  }
  return c;
}

// 8-connected clustering of the significant pixels, largest clusters first
std::vector<Cluster> findClusters(const std::vector<uint8_t>& mask, std::size_t w, std::size_t h)
{
  std::vector<int> label(w * h, 0);
  std::vector<Cluster> clusters;
  for (int y = 0; y < (int)h; ++y)
    for (int x = 0; x < (int)w; ++x)
    {
      const std::size_t idx = std::size_t(y) * w + x;
      if (mask[idx] != 2 || label[idx])
        continue;
      clusters.push_back(fillCluster(mask, label, w, h, x, y, (int)clusters.size() + 1));
    }

  std::sort(clusters.begin(),
            clusters.end(),
            [](const Cluster& p, const Cluster& q) { return p.area > q.area; });
  return clusters;
}

// Collect the statistics of the significant clusters
void summarizeClusters(const std::vector<Cluster>& clusters,
                       const StructuralDiffOptions& opts,
                       StructuralDiffResult& res)
{
  for (const auto& c : clusters)
  {
    if (c.area < opts.minClusterArea)
      continue;
    ++res.clusterCount;
    res.significantPixels += c.area;
    if (c.area >= res.largestClusterArea)
    {
      res.largestClusterArea = c.area;
      res.boxX = c.minx;
      res.boxY = c.miny;
      res.boxW = c.maxx - c.minx + 1;
      res.boxH = c.maxy - c.miny + 1;
    }
    if (c.area >= opts.failClusterArea)
      res.fail = true;
  }
}

// Keep only the pixels whose whole (2r+1)x(2r+1) neighbourhood is set
std::vector<uint8_t> erode(const std::vector<uint8_t>& mask, std::size_t w, std::size_t h, int r)
{
  if (r <= 0)
    return mask;
  // Separable erosion: first horizontally, then vertically
  std::vector<uint8_t> tmp(w * h, 0);
  std::vector<uint8_t> out(w * h, 0);
  for (int y = 0; y < (int)h; ++y)
    for (int x = r; x < (int)w - r; ++x)
    {
      bool all = true;
      for (int dx = -r; dx <= r && all; ++dx)
        all = (mask[std::size_t(y) * w + x + dx] != 0);
      tmp[std::size_t(y) * w + x] = all ? 2 : 0;
    }
  for (int y = r; y < (int)h - r; ++y)
    for (int x = 0; x < (int)w; ++x)
    {
      bool all = true;
      for (int dy = -r; dy <= r && all; ++dy)
        all = (tmp[std::size_t(y + dy) * w + x] != 0);
      out[std::size_t(y) * w + x] = all ? 2 : 0;
    }
  return out;
}

// Fail on a large connected area change
void summarizeAreas(const std::vector<Cluster>& areas,
                    std::size_t w,
                    std::size_t h,
                    const StructuralDiffOptions& opts,
                    StructuralDiffResult& res)
{
  if (areas.empty())
    return;
  const auto& c = areas.front();
  res.largestArea = c.area;
  res.areaX = c.minx;
  res.areaY = c.miny;
  res.areaW = c.maxx - c.minx + 1;
  res.areaH = c.maxy - c.miny + 1;
  if (opts.maxAreaFraction > 0 && c.area >= opts.areaMinPixels &&
      c.area >= (long)(opts.maxAreaFraction * double(w) * double(h)))
  {
    res.areaChange = true;
    res.fail = true;
  }
}

// Set the overlay color of a pixel
void setColor(std::vector<uint8_t>& out, std::size_t i, uint8_t r, uint8_t g, uint8_t b)
{
  out[i * 4] = r;
  out[i * 4 + 1] = g;
  out[i * 4 + 2] = b;
}

// Write an overlay image of the differences on a washed out version of the second image
void writeOverlay(const std::filesystem::path& overlayPath,
                  const std::vector<uint8_t>& b,
                  const std::vector<uint8_t>& mask,
                  const std::vector<uint8_t>& areaMask,
                  const std::vector<Cluster>& clusters,
                  const StructuralDiffResult& res,
                  std::size_t w,
                  std::size_t h,
                  const StructuralDiffOptions& opts)
{
  std::vector<uint8_t> out(w * h * 4);
  for (std::size_t i = 0; i < w * h; ++i)
  {
    double g = 0.30 * b[i * 4] + 0.59 * b[i * 4 + 1] + 0.11 * b[i * 4 + 2];
    g = 200.0 + g * 0.2;  // wash out for low-contrast context
    const uint8_t gv = (uint8_t)std::min(255.0, g);
    if (mask[i] == 2)
      setColor(out, i, 255, 0, 0);
    else if (mask[i] == 1)
      setColor(out, i, 190, 210, 255);
    else if (areaMask[i] != 0 && res.areaChange)
      setColor(out, i, 255, 190, 120);
    else
      setColor(out, i, gv, gv, gv);
    out[i * 4 + 3] = 255;
  }
  Magick::Image od(w, h, "RGBA", Magick::CharPixel, out.data());
  od.strokeColor("red");
  od.fillColor(Magick::Color("transparent"));
  od.strokeWidth(2);
  // Draw each box with the single-Drawable overload: the batch draw()
  // takes std::list on ImageMagick 6 and std::vector on 7 (no common
  // container type), whereas draw(const Drawable&) exists in both.
  for (const auto& c : clusters)
    if (c.area >= opts.minClusterArea)
      od.draw(Magick::DrawableRectangle(std::max(0, c.minx - 3),
                                        std::max(0, c.miny - 3),
                                        std::min((int)w - 1, c.maxx + 3),
                                        std::min((int)h - 1, c.maxy + 3)));
  if (res.areaChange)
  {
    od.strokeColor("orange");
    od.draw(Magick::DrawableRectangle(std::max(0, res.areaX - 3),
                                      std::max(0, res.areaY - 3),
                                      std::min((int)w - 1, res.areaX + res.areaW + 2),
                                      std::min((int)h - 1, res.areaY + res.areaH + 2)));
  }
  od.write(overlayPath.string());
}

}  // namespace

StructuralDiffResult structuralImageDiff(const std::filesystem::path& image1,
                                         const std::filesystem::path& image2,
                                         std::optional<std::filesystem::path> overlayPath,
                                         const StructuralDiffOptions& opts)
{
  StructuralDiffResult res;

  Magick::Image im1(image1.string());
  Magick::Image im2(image2.string());
  const std::size_t w = im1.columns();
  const std::size_t h = im1.rows();
  res.width = w;
  res.height = h;
  if (w != im2.columns() || h != im2.rows())
  {
    res.sizeMismatch = true;
    res.fail = true;
    return res;
  }

  std::vector<uint8_t> a(w * h * 4);
  std::vector<uint8_t> b(w * h * 4);
  im1.write(0, 0, w, h, "RGBA", Magick::CharPixel, a.data());
  im2.write(0, 0, w, h, "RGBA", Magick::CharPixel, b.data());

  // mask: 0 = same, 1 = weak/AA difference, 2 = significant difference
  std::vector<uint8_t> areaMask;
  const auto mask = classifyPixels(a, b, w, h, opts, areaMask, res);

  // 8-connected clustering of the significant pixels.
  const auto clusters = findClusters(mask, w, h);

  summarizeClusters(clusters, opts, res);

  // 8-connected clustering of the area change pixels (see StructuralDiffOptions::maxAreaFraction)
  summarizeAreas(findClusters(erode(areaMask, w, h, opts.areaErosion), w, h), w, h, opts, res);

  // Anti-aliasing flood guard: a sub-pixel geometry displacement is misread
  // pixel-by-pixel as anti-aliasing, so it never forms a "significant"
  // cluster, yet it floods the AA-ignored count far beyond a genuine per-edge
  // fringe. Fail when the AA-classified pixels exceed both an absolute floor
  // and a fraction of the frame (see StructuralDiffOptions::maxAaFraction).
  if (opts.maxAaFraction > 0 && res.aaIgnoredPixels >= opts.aaFloodMinPixels &&
      res.aaIgnoredPixels >= (long)(opts.maxAaFraction * double(w) * double(h)))
  {
    res.aaFlood = true;
    res.fail = true;
  }

  if (overlayPath)
    writeOverlay(*overlayPath, b, mask, areaMask, clusters, res, w, h, opts);

  return res;
}

}  // namespace Fmi
