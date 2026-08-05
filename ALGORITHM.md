# Marker Detection and Coverage Algorithm

This document describes the current marker-analysis pipeline.

The input is a BGR image that may contain a colored 3×3 marker. The output
contains:

- a detection status;
- an optional geometry warning;
- a coverage ratio when a nine-cell grid was successfully constructed;
- intermediate information for debugging and visualization.

## 1. Color segmentation

The image is converted from BGR to HSV and lightly blurred.

Separate masks are created for:

- red;
- green;
- blue;
- yellow;
- cyan;
- magenta.

Red uses two hue intervals because its hue range wraps around the HSV scale.

Each mask is cleaned using:

1. morphological opening, to remove small isolated regions;
2. morphological closing, to fill small gaps.

External contours are extracted from each cleaned mask.

Every accepted contour becomes a `Patch` containing:

- the classified color;
- an axis-aligned bounding rectangle;
- a center computed from image moments;
- contour area;
- a stable integer identifier.

Contour areas are filtered relative to the full image area.

Current defaults:

```text
minimum contour area = 0.0006 × image area
maximum contour area = 0.2    × image area
```

## 2. PCA coordinate normalization

The center of every segmentation candidate participates in PCA.

The two PCA eigenvectors define a rotated coordinate system:

```text
original center (x, y)
        │
        ▼
rotated center (x', y')
```

The goal is to align the candidate distribution with its dominant axes before
row and column clustering.

This improves tolerance to in-plane rotation. It does not perform projective
rectification or homography estimation.

## 3. Deterministic row and column clustering

OpenCV K-means is applied independently to:

- every `y'` value, producing three row clusters;
- every `x'` value, producing three column clusters.

Each invocation uses:

```text
clusters:               3
initialization:          KMEANS_PP_CENTERS
attempts:                5
maximum iterations:      100
epsilon:                 0.001
```

`KMEANS_PP_CENTERS` uses OpenCV's random-number generator. The detector resets
the OpenCV seed before each clustering operation so that:

- repeated runs produce identical output;
- processing a picture alone or inside a batch gives the same result;
- reversing batch order does not change per-image output.

A fixed seed makes one K-means solution reproducible. It does not guarantee a
globally optimal clustering.

## 4. Greedy 3×3 assignment

The three row centers and three column centers create nine target
intersections.

For every patch and every grid cell, the detector computes a distance in PCA
coordinates:

```text
distance = |patch_x' - column_center|
         + |patch_y' - row_center|
```

The distance receives a small reduction when the patch's K-means labels match
the candidate row or column.

All patch-to-cell candidates are sorted by distance. The algorithm greedily
selects candidates while enforcing:

- one patch per grid cell;
- one grid cell per patch.

The assignment succeeds only when all nine cells are filled.

This greedy stage is a known limitation: the locally closest candidate is not
always part of the globally best nine-patch grid.

## 5. Spacing validation

For each selected row:

1. sort its three patches by `x'`;
2. compute the two horizontal gaps;
3. divide both gaps by their local mean.

For each selected column, the same process is applied to `y'`.

This produces six normalized horizontal distances and six normalized vertical
distances.

The detector calculates:

```text
cvx = stdev(horizontal normalized gaps)
      / mean(horizontal normalized gaps)

cvy = stdev(vertical normalized gaps)
      / mean(vertical normalized gaps)
```

Current thresholds:

| Status | `cvx` | `cvy` |
|---|---:|---:|
| Strict detection | ≤ 0.55 | ≤ 0.65 |
| Detection with warning | ≤ 0.70 | ≤ 0.70 |
| Not detected | exceeds warning threshold | exceeds warning threshold |

The warning thresholds represent heuristic tolerance, not a statistical
confidence score.

## 6. Coverage estimation

After a nine-patch grid is constructed, the estimator collects all four
corners of each selected patch's bounding rectangle.

This creates at most 36 points.

OpenCV computes their convex hull and its area:

```text
coverage_ratio = convex_hull_area / image_area
```

Coverage describes how much of the image is occupied by the selected marker.

It is not used to decide whether the marker exists in normal mode.

## 7. Detection status

Normal mode returns:

### `DETECTED`

- all nine grid cells were assigned;
- coverage geometry is valid;
- strict spacing validation passed.

### `DETECTED_WITH_WARNING`

- all nine grid cells were assigned;
- coverage geometry is valid;
- strict spacing failed;
- tolerant spacing passed.

### `NOT_DETECTED`

Examples include:

- fewer than three segmentation candidates;
- grid assignment failure;
- invalid hull or image geometry;
- spacing exceeds both strict and tolerant thresholds.

A valid image without an accepted marker is still a successful program
execution. The CLI therefore exits with code `0` after completing the
analysis.

## 8. Legacy mode

`--legacy-output` retains the original assignment decision rules for
regression comparison.

The original model mixed coverage and detection:

```text
coverage threshold: 0.45
coverage fallback:  0.55
soft coverage:      0.50
```

Coverage could therefore accept an irregular grid or reject a geometrically
regular but small marker.

This behavior is intentionally isolated from the normal detection model.

## 9. Diagnostics

`--debug` prints:

- candidate count;
- grid-assignment result;
- strict and tolerant spacing results;
- `cvx` and `cvy`;
- coverage;
- hull and image areas;
- legacy result;
- elapsed processing time.

`--visualize-dir` saves an annotated PNG showing:

- all segmentation candidates;
- the selected grid;
- assigned row and column labels;
- the coverage convex hull;
- detection metrics.

## 10. Complexity

Let:

- `P` be the number of image pixels;
- `N` be the number of segmentation candidates;
- `I` be the bounded number of K-means iterations.

Approximate costs are:

- HSV conversion, masks and morphology: `O(P)`;
- contour processing: proportional to mask and contour size;
- PCA: `O(N)`;
- row and column K-means: `O(N × I)`, with three clusters;
- candidate generation and sorting: `O(N log N)` for a fixed nine cells;
- convex hull: effectively constant-sized because it receives at most
  36 rectangle corners.

For the supplied images, `N` is small.

## 11. Known limitations

- fixed HSV thresholds;
- no illumination adaptation;
- PCA includes false positives;
- clustering includes every candidate;
- greedy rather than global assignment;
- no expected color-layout validation;
- no explicit perspective correction;
- axis-aligned patch rectangles;
- heuristic spacing thresholds;
- regression dataset is small and does not represent unseen-image accuracy.
