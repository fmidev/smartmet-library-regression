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
4. Cluster surviving pixels (8-connectivity).
5. **Fail iff some cluster is large enough** (`SID_FAIL_CLUSTER`, default 80 px).
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

The default cutoff of 80 sits in the gap with margin on both sides. Even a
complete font swap (far more extreme than any RHEL8→Rocky10 difference) stays at
10, while the smallest real change is 169 — a 16× margin.

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

## Output

```
$ smartimagediff result.png expected.png overlay.png
OK    largest_cluster=10 clusters=0 changed_px=0 aa_ignored=31713
FAIL  largest_cluster=169 clusters=1 changed_px=169 aa_ignored=18 at=[530,501 11x17]
```

Exit code 0 = OK, 1 = regression, 2 = error. The `at=[x,y WxH]` box points the
inspector straight at the change. The optional overlay PNG shows the expected
image faded to faint grayscale with the real change painted red and boxed —
instead of an equalized full-frame of antialiasing.

## Tunables (environment)

| var                | default | meaning                                       |
|--------------------|---------|-----------------------------------------------|
| `SID_THRESHOLD`    | 0.1     | base per-pixel sensitivity (pixelmatch)       |
| `SID_STRONG`       | 0.25    | normalized amplitude for a "significant" pixel |
| `SID_MIN_CLUSTER`  | 8       | clusters smaller than this counted as noise    |
| `SID_FAIL_CLUSTER` | 80      | a cluster ≥ this is a regression               |

## Known floor

A change smaller than ~`SID_FAIL_CLUSTER` strong pixels (e.g. a single missing
minus sign or decimal point, ~10–15 px) is indistinguishable from font jitter by
area alone and will pass. This is strictly better than PSNR, which silently
passes such changes too (they barely move the average). Lower `SID_FAIL_CLUSTER`
to ~30 to catch smaller changes at the cost of less headroom against an
unrealistically different rasterizer.

## Integration into CompareImages.pl

Drop-in for the final PNG comparison block (the `smartimagediff_psnr` call near
the end). Replace:

```perl
my $difference_str = `smartimagediff_psnr $RESULT_PNG $EXPECTED_PNG $DIFFERENCE_PNG`;
# ...PSNR parsing, >=50 OK / >=20 WARNING / <20 FAIL...
```

with:

```perl
my $out = `smartimagediff $RESULT_PNG $EXPECTED_PNG $DIFFERENCE_PNG`;
my $rc  = $? >> 8;
if ($rc == 0) { print "OK     "; Cleanup(); exit(0); }
elsif ($rc == 1) {
    print "FAIL   $out";
    CreateWebp("failures/${NAME}_error.webp", $EXPECTED_PNG, $RESULT_PNG);
    exit(1);
}
else { die "smartimagediff error: $out"; }
```

The WARNING tier disappears: the tool decides. Keep both binaries installed
during a transition period and compare verdicts on a real test run before
switching the suite over.
