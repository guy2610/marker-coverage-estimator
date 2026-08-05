# Detection Model

## Overview

Marker Coverage Estimator performs two related but distinct tasks:

1. Determine whether the segmented color patches form a plausible 3×3 marker.
2. Estimate how much of the image is covered by the selected marker.

The original implementation combined these decisions by requiring a minimum
coverage ratio before reporting a successful detection. The current model
separates marker validity from marker size.

A correctly detected marker may therefore cover less than 45% of the image.

## Detection statuses

The analysis pipeline returns one of three statuses:

- `DETECTED`
  - Nine patches were assigned to a 3×3 grid.
  - The strict spacing thresholds passed.

- `DETECTED_WITH_WARNING`
  - Nine patches were assigned to a 3×3 grid.
  - Strict spacing validation failed.
  - The more tolerant spacing thresholds passed.
  - Coverage is still reported, together with an irregular-spacing warning.

- `NOT_DETECTED`
  - Grid assignment failed, coverage geometry was invalid, or spacing exceeded
    both the strict and tolerant thresholds.

A valid image without a marker is an analysis result, not a program execution
error. Normal analysis therefore exits with status code `0` even when one or
more images contain no accepted marker.

## Geometry thresholds

The current thresholds are heuristic:

| Metric | Strict | Warning |
|---|---:|---:|
| Horizontal spacing coefficient of variation (`cvx`) | 0.55 | 0.70 |
| Vertical spacing coefficient of variation (`cvy`) | 0.65 | 0.70 |

The strict thresholds preserve the original geometry checks.

The warning thresholds distinguish a moderately irregular grid from a clear
spacing failure. They should not be interpreted as confidence probabilities.

## Coverage

Coverage is computed only after nine patches have been assigned to the grid.

The estimator:

1. Collects the four corners of every selected patch bounding box.
2. Computes their outer convex hull.
3. Divides the hull area by the full image area.

```text
coverage_ratio = convex_hull_area / image_area
```

Coverage describes marker size. It does not determine whether the marker
exists.

## Deterministic clustering

The detector uses OpenCV K-means independently on PCA-rotated horizontal and
vertical coordinates.

OpenCV's `KMEANS_PP_CENTERS` initialization uses a random-number generator.
The original implementation therefore produced slightly different grid
assignments depending on prior K-means calls and image processing order.

The current implementation resets the OpenCV random seed before each row and
column clustering operation. This provides:

- identical results across repeated runs;
- per-image results independent of batch order;
- reproducible debugging and regression checks.

A fixed seed makes the selected local solution reproducible. It does not prove
that the selected clustering is globally optimal.

## Legacy compatibility

The `--legacy-output` option preserves the original decision rules, including
the coverage threshold and coverage-based geometry fallbacks.

Example:

```bash
./build/marker-coverage \
  --legacy-output \
  data/hi1.png \
  data/hi8.png
```

The original accepted/rejected result remains 18 accepted and 2 rejected on
the supplied 20-image dataset.

Individual coverage values can differ slightly from the historical captured
output because the historical K-means initialization was nondeterministic.
For example, deterministic assignments currently change some rounded values
such as `hi1` and `hi11`, while preserving the legacy acceptance decisions.

## Visual diagnostics

Annotated images can be generated with:

```bash
./build/marker-coverage \
  --debug \
  --visualize-dir output/debug \
  data/hi6.png \
  data/hi8.png
```

The visualization contains:

- thin yellow rectangles: all segmentation candidates;
- thick colored rectangles: the nine selected grid patches;
- `rNcM` labels: assigned row and column;
- pink outline: the convex hull used for coverage;
- status, candidate count, spacing metrics and coverage.

## Current deterministic dataset result

The current normal-mode result on the supplied dataset is:

```text
detected=15
warnings=2
not_detected=3
total=20
```

The warning cases are currently `hi11` and `hi17`.

The rejected cases are currently `hi6`, `hi7` and `hi13`.

## Known limitations

The diagnostic visualizations expose several limitations:

- Fixed HSV thresholds may produce false-positive color patches.
- PCA is computed from every segmentation candidate and can be influenced by
  noise.
- Row and column clustering includes all candidates, not only the true marker.
- Final grid-cell selection uses a greedy assignment.
- One incorrectly selected patch can distort spacing metrics and the coverage
  hull.
- The coverage hull uses axis-aligned contour bounding boxes.
- The detector does not validate an expected marker color arrangement.
- The strict and warning thresholds are heuristic and dataset-dependent.

The next algorithmic improvement should focus on candidate selection and global
grid assignment rather than merely widening spacing thresholds.
