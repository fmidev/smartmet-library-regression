# Structural image diff (`smartimagediff`)

A second image comparator alongside `smartimagediff_psnr`, built to fix a
specific pain point in the WMS regression suite: too many manual inspections
caused by harmless rendering differences, while real changes can score *better*
than the noise.

## The problem with global PSNR

`smartimagediff_psnr` reduces the whole image to one number (peak SNR from mean
squared error) and then `equalize()`s the difference image for display. Two
consequences:

1. **A global average cannot separate the two things we care about.** Measured
   on real WMS output (`ecmwf_data_numbers`, `feelslike` rasterized at 1024px):

   | case (expected vs result)             | PSNR (dB) | verdict today    |
   |---------------------------------------|-----------|------------------|
   | pure anti-aliasing jitter (rerender)  | 28–32     | WARNING (manual) |
   | **real missing character**            | **41**    | WARNING (manual) |
   | real missing map feature              | 28.3      | WARNING (manual) |
   | real colour regression                | 23.2      | WARNING (manual) |

   A missing character (41 dB) scores **better** than harmless AA noise (28 dB),
   and AA-only (28.07) scores **worse** than a genuinely missing feature
   (28.34). No PSNR threshold can ever put the "ignore" cases on one side and
   the "catch" cases on the other.

2. **`equalize()` amplifies the noise it should hide.** For a pure-AA difference
   with *zero* real change, every glyph and every contour line lights up, so the
   inspector must study a full-frame of noise to conclude "nothing changed".

The accepted images were rendered on RHEL8; tests now run on Rocky 10, so
cross-OS anti-aliasing / freetype-hinting differences are expected and must be
tolerated.

## The approach

Judge on the **geometry of the differences**, not their average:

1. Per-pixel YIQ perceptual colour delta (pixelmatch / Yee metric).
2. Discard anti-aliasing-class pixels (Vyšniauskas detector).
3. Keep only **high-amplitude** differences (`SID_STRONG`, default 0.25). This
   is the key step: a dark glyph on a light isoband is a large colour delta,
   whereas font-hinting jitter is a small gray-vs-gray edge delta. Raising the
   bar past ~0.30 starts dropping real glyph signal too, so 0.25 is the sweet
   spot.
4. Tolerate displacements of 1-2 pixels (`SID_DISPLACEMENT_RADIUS`, default 2):
   a significant pixel is dropped when the colour of each image at it is found
   within the radius in the other image. Text drawn by another version of the
   font libraries, or geometry shifted by a pixel, passes; a missing feature or
   a changed colour has no counterpart nearby.
5. Cluster surviving pixels (8-connectivity).
6. **Fail iff some cluster is large enough** (`SID_FAIL_CLUSTER`, default 40 px).
   Rendering jitter only ever produces thin per-edge specks; a real change forms
   one solid blob.

## Measured separation (largest connected cluster, `SID_STRONG=0.25`)

| should be IGNORED                          | largest cluster |
|--------------------------------------------|-----------------|
| sub-pixel rerender jitter (map)            | 4               |
| sub-pixel rerender jitter (numbers)        | 6               |
| edge-softening blur (numbers / map)        | 0               |
| **full font substitution** Roboto→DejaVu vs →Droid | **10**  |
| different SVG rasterizer entirely (stress) | 45              |

| should be CAUGHT                           | largest cluster |
|--------------------------------------------|-----------------|
| single erased thin glyph ("1")             | 169             |
| single erased wide glyph                   | 216             |
| missing map feature                        | 3700            |
| colour regression                          | 12053           |

These were measured without the displacement tolerance and with a cutoff of 80.
Rendering the whole WMS suite on RHEL 8 and RHEL 10 with the same fonts gives
clusters of up to 318 pixels around text, as glyph shapes and advances differ
between the freetype, harfbuzz and librsvg versions. With the displacement
tolerance the largest remaining cluster between the platforms is 27 pixels and
the largest between RHEL 8 renders months apart 31 pixels, hence the cutoff of
40. Every RHEL 10 image of the suite then passes against the RHEL 8 expected
images, while a missing lake fill (173 px), the pale area changes and replaced
maps below still fail.

## Validation on the real product catalog

Rasterizing 40 diverse WMS SVG products and comparing each against a sub-pixel
edge-softened copy (the faithful cross-OS anti-aliasing proxy — same geometry,
differing only in edge smoothing):

```
40/40 OK, 0 false positives
```

A harsher proxy — a 0.5 px *rigid translation of the whole image* — is flagged
on 39/40. That is **by design**: a global sub-pixel shift is a real geometric
change (every edge moved coherently), not anti-aliasing. Which leads to the one
assumption to be aware of:

> The comparator assumes both images are on the **same pixel grid** — i.e. they
> differ only in how identical geometry was anti-aliased/hinted, not in where
> that geometry landed. This holds for the WMS suite, where both sides come from
> the same SVG coordinates rasterized by the same `rsvg-convert`; cross-OS
> differences are AA/hinting, not translation. If a renderer ever introduced a
> consistent sub-pixel viewport offset, every test would flag — but so would a
> human, and it would be a genuine positioning regression. (The existing PSNR
> tool shares the same pixel-aligned assumption.)

## Area changes in pale colours

The amplitude bar of step 3 has a blind spot: pale colours. Meteorological
backgrounds and isoband palettes use colours that differ by less than the base
threshold, so a whole recoloured area produces no candidate pixels at all:

| colour pair                              | normalized YIQ difference |
|------------------------------------------|---------------------------|
| meteo-light land vs sea                  | 0.085 (below `SID_THRESHOLD`) |
| meteo-dark land vs sea                   | 0.058 |
| adjacent light-blue isoband colours      | 0.099 |
| protomaps-light land vs water            | 0.216 (below `SID_STRONG`) |
| yellow vs light green precipitation      | 0.239 |

Recolouring one band of `wms_getmap_isoband_style1` to its neighbour's colour
or deleting a pale band passed with **0 changed pixels** (PSNR 28.5–36.7 dB),
and replacing a fictional OSM test tile with a real map of Helsinki (81% of the
pixels changed) passed too.

Such a change is one large solid area, whereas rendering differences stay thin.
So a second, independent check clusters the pixels differing by more than
`SID_AREA_THRESHOLD` (default 0.03) that are not anti-aliasing, after eroding
them by `SID_AREA_EROSION` pixels (default 2) so that softened or displaced
edges and thin lines vanish. A cluster of at least `SID_MAX_AREA_FRACTION` of
the image (default 0.002) and `SID_AREA_MIN` pixels (default 200) fails the
comparison, and the output ends with `area_change`. The threshold 0.03 stays
above uniform palette quantization shifts (an 8 level shift of every channel is
about 0.03).

Measured largest area after erosion, as a fraction of the image:

| case                                                     | largest area |
|----------------------------------------------------------|--------------|
| whole WMS suite on el8 (120 image tests differing from the reference) | ≤ 33 px, 0.022% |
| whole WMS suite on el10 (138 image tests differing from the reference) | ≤ 62 px, 0.003% |
| recoloured or deleted pale isoband (4 variants)           | 0.86–1.4%    |
| OSM map replaced (9 tests)                                | 2.1–45%      |

The default 0.2% leaves a margin of about 9 on the jitter side and 4 on the
regression side.

## Output

```
$ smartimagediff result.png expected.png overlay.png
OK    largest_cluster=10 clusters=0 changed_px=0 aa_ignored=31713 displaced=0 largest_area=0
FAIL  largest_cluster=169 clusters=1 changed_px=169 aa_ignored=18 displaced=0 at=[530,501 11x17] largest_area=0
FAIL  largest_cluster=0 clusters=0 changed_px=0 aa_ignored=0 displaced=0 largest_area=6159 area_at=[156,141 126x188] area_change
```

Images of different sizes print `FAIL  size mismatch WxH`.
Exit code 0 = OK, 1 = regression, 2 = error. The `at=[x,y WxH]` box points the
inspector straight at the change, `area_at=[x,y WxH]` at the largest area change. The optional overlay PNG shows the expected
image faded to faint grayscale with the real change painted red and boxed —
instead of an equalized full-frame of antialiasing.

## Tunables (environment)

| var                | default | meaning                                       |
|--------------------|---------|-----------------------------------------------|
| `SID_THRESHOLD`    | 0.1     | base per-pixel sensitivity (pixelmatch)       |
| `SID_STRONG`       | 0.25    | normalized amplitude for a "significant" pixel |
| `SID_MIN_CLUSTER`  | 8       | clusters smaller than this counted as noise    |
| `SID_FAIL_CLUSTER` | 40      | a cluster ≥ this is a regression               |
| `SID_MAX_AA_FRACTION` | 0     | anti-aliasing flood guard: fraction of the image area, for example 0.003 (≤ 0 disables) |
| `SID_DISPLACEMENT_RADIUS` | 2 | displacement tolerance: search radius in pixels (≤ 0 disables) |
| `SID_DISPLACEMENT_TOLERANCE` | 0.1 | displacement tolerance: normalized amplitude of matching colours |
| `SID_AA_FLOOD_MIN` | 1000    | anti-aliasing flood guard: absolute pixel floor |
| `SID_AREA_THRESHOLD` | 0.03  | area change check: normalized amplitude of a changed pixel |
| `SID_AREA_EROSION` | 2       | area change check: erosion radius in pixels   |
| `SID_MAX_AREA_FRACTION` | 0.002 | area change check: fraction of the image area (≤ 0 disables) |
| `SID_AREA_MIN`     | 200     | area change check: absolute pixel floor        |

The anti-aliasing flood guard catches a thin line that moved by about one pixel
along its whole length: such a change looks like anti-aliasing pixel by pixel.
Genuine anti-aliasing stays a thin fringe, so when the pixels classified as
anti-aliasing exceed both `SID_AA_FLOOD_MIN` and `SID_MAX_AA_FRACTION` of the
image, the comparison fails and the output line ends with `aa_flood`. The guard
is disabled by default: displacements of 1-2 pixels must not fail the tests, and
text rendered by other font library versions floods the anti-aliasing count just
like a moved line. Set for example `SID_MAX_AA_FRACTION=0.003` to enable it.

## Known floor

A change smaller than ~`SID_FAIL_CLUSTER` strong pixels (e.g. a single missing
minus sign or decimal point, ~10–15 px, or an erased digit of small text, ~30 px)
is indistinguishable from font jitter by area alone and will pass, and so does a
digit replaced by a similar one (5 → 6), which the displacement tolerance
explains. This is strictly better than PSNR, which silently passes such changes
too (they barely move the average). Test values of text in the SVG or JSON
outputs instead.

## Integration into CompareImages.pl

The WMS plugin's `test/CompareImages.pl` uses `smartimagediff` as the gate: exit
code 0 is OK, 1 is FAIL, and the overlay is written to
`failures/<name>_overlay.png` together with an expected/result flicker WebP.
The PSNR of `smartimagediff_psnr` is printed for information. When
`smartimagediff` is not installed, the script falls back to the PSNR verdict
(>= 50 dB OK, >= 20 dB WARNING, otherwise FAIL).
