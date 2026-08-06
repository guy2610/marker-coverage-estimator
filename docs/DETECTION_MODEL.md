# Detection Model

## Overview

Marker Coverage Estimator performs two separate tasks:

1. Determine whether segmented color patches form a plausible 3×3 marker.
2. Estimate how much of the image is covered by the selected marker.

Marker size does not determine marker validity. A geometrically regular marker
can therefore be detected even when it occupies a small part of the image.

## Detection statuses

The analysis pipeline returns one of three statuses:

### `DETECTED`

- Nine patches were assigned to a 3×3 grid.
- Coverage geometry is valid.
- Strict spacing thresholds passed.

### `DETECTED_WITH_WARNING`

- Nine patches were assigned to a 3×3 grid.
- Coverage geometry is valid.
- Strict spacing validation failed.
- Tolerant spacing validation passed.

Coverage is reported together with an irregular-spacing warning.

### `NOT_DETECTED`

Examples include:

- insufficient segmentation candidates;
- grid-assignment failure;
- invalid coverage geometry;
- spacing that exceeds both strict and tolerant thresholds.

A valid image without an accepted marker is an analysis result, not a program
execution failure. Normal analysis therefore exits with status code `0`.

## Geometry thresholds

| Metric | Strict | Warning |
|---|---:|---:|
| Horizontal spacing coefficient of variation (`cvx`) | 0.55 | 0.70 |
| Vertical spacing coefficient of variation (`cvy`) | 0.65 | 0.70 |

These are heuristic thresholds, not probabilities or calibrated confidence
scores.

## Coverage

Coverage is calculated after nine patches have been assigned:

1. Collect the four corners of each selected patch rectangle.
2. Compute their outer convex hull.
3. Divide the hull area by the complete image area.

```text
coverage_ratio = convex_hull_area / image_area
```

Because the current implementation uses axis-aligned contour rectangles, its
reported coverage can exceed the polygon ground truth of a rotated synthetic
marker. This is a documented estimator limitation.

## Deterministic clustering

OpenCV K-means is run independently on PCA-rotated horizontal and vertical
coordinates.

`KMEANS_PP_CENTERS` uses a random-number generator. The implementation resets
the OpenCV seed before each clustering operation, providing:

- identical output across repeated runs;
- results independent of image batch order;
- reproducible debugging and regression checks.

A fixed seed makes one local clustering solution reproducible. It does not
guarantee that the selected clustering is globally optimal.

## Public regression cases

The public tests generate their input images at runtime.

Stable cases currently include:

- centered marker;
- low-coverage marker;
- positive and negative in-plane rotation;
- a controlled perspective transform;
- irregular spacing that produces a warning;
- a missing-patch negative;
- a blank-image negative.

Blur/noise and colored-distractor scenes are generated as challenge cases but
are not required to pass the current regression suite.

See [DATASET.md](DATASET.md) for dataset provenance.
