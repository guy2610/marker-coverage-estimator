# Marker Coverage Estimator

[![CI](https://github.com/guy2610/marker-coverage-estimator/actions/workflows/ci.yml/badge.svg?branch=portfolio-upgrade)](https://github.com/guy2610/marker-coverage-estimator/actions/workflows/ci.yml)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![OpenCV](https://img.shields.io/badge/OpenCV-4.x-green.svg)

A deterministic C++17/OpenCV pipeline that detects a colored 3×3 marker,
validates its geometry, and estimates how much of an image it covers.

The project started as a computer-vision assignment and was later reworked into
a portable, tested command-line application with explicit detection semantics,
diagnostic visualizations, regression tests, and Linux CI.

## Visual diagnostics

| Detected | Detected with warning | Not detected |
|---|---|---|
| <img src="docs/images/hi8_annotated.png" width="290" alt="Strict marker detection"> | <img src="docs/images/hi11_annotated.png" width="290" alt="Marker detection with geometry warning"> | <img src="docs/images/hi6_annotated.png" width="290" alt="Rejected marker candidate"> |
| Regular 3×3 geometry, even though coverage is only 38.6%. | Marker retained with an irregular-spacing warning. | Candidate grid rejected because vertical spacing is too irregular. |

Visualization legend:

- thin yellow boxes: every color-segmentation candidate;
- thick colored boxes: the nine patches selected for the grid;
- `rNcM` labels: assigned row and column;
- pink outline: the convex hull used for coverage.

## Why detection and coverage are separate

The original implementation required a minimum coverage ratio before reporting
a marker. That caused a geometrically valid marker such as `hi8` to be reported
as `0%` simply because it occupied less than 45% of the image.

The current model treats these as separate questions:

1. Do the selected patches form a plausible 3×3 marker?
2. What percentage of the image does that marker cover?

A valid small marker can therefore be detected and still report low coverage.

## Detection statuses

The normal CLI returns one of three states:

- `DETECTED` — the selected grid passes strict spacing validation;
- `DETECTED_WITH_WARNING` — the grid passes tolerant, but not strict,
  spacing validation;
- `NOT_DETECTED` — assignment or geometry validation fails.

Coverage is reported for both successful states.

## Processing pipeline

```text
BGR image
   │
   ▼
HSV color segmentation
   │
   ▼
Contour and relative-area filtering
   │
   ▼
PCA rotation normalization
   │
   ▼
Deterministic row/column K-means
   │
   ▼
Greedy assignment to a 3×3 grid
   │
   ├── spacing validation ──► detection status
   │
   └── convex hull ─────────► image coverage
```

At a high level:

1. Convert BGR to HSV and create masks for red, green, blue, yellow, cyan,
   and magenta.
2. Clean each mask with morphological opening and closing.
3. Extract contour candidates and filter them by image-relative area.
4. Rotate patch centers into PCA coordinates.
5. Cluster horizontal and vertical coordinates into three rows and three
   columns.
6. Assign nine candidates to grid cells.
7. Measure spacing consistency using coefficients of variation.
8. Compute coverage from the convex hull of the selected patch rectangles.

See [ALGORITHM.md](ALGORITHM.md) for the detailed algorithm and
[docs/DETECTION_MODEL.md](docs/DETECTION_MODEL.md) for the status model.

## Build

### Requirements

- CMake 3.16 or newer
- C++17 compiler
- OpenCV 4
- Python 3 when building the regression tests

### Ubuntu

```bash
sudo apt-get update
sudo apt-get install --yes \
  cmake \
  g++ \
  libopencv-dev \
  ninja-build

cmake \
  -S . \
  -B build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON

cmake --build build --parallel
```

### macOS with Homebrew

```bash
brew install cmake opencv@4

cmake \
  -S . \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DOpenCV_DIR="$(brew --prefix opencv@4)/lib/cmake/opencv4"

cmake --build build --parallel
```

## Usage

Analyze one or more images:

```bash
./build/marker-coverage \
  data/hi8.png \
  data/hi11.png
```

Example output:

```text
data/hi8.png marker_found coverage=38.6%
data/hi11.png marker_found coverage=57.0% warning=IRREGULAR_SPACING

Summary: detected=1 warnings=1 not_detected=0 out of 2
```

Print detailed metrics:

```bash
./build/marker-coverage \
  --debug \
  data/hi8.png
```

Generate annotated images:

```bash
./build/marker-coverage \
  --visualize-dir output/debug \
  data/hi6.png \
  data/hi8.png
```

Show all CLI options:

```bash
./build/marker-coverage --help
```

## Legacy comparison

The original implementation is preserved at the annotated tag:

```text
v0.1.0-baseline
```

The compatibility mode reproduces the original acceptance rules, where
coverage participated in the detection decision:

```bash
./build/marker-coverage \
  --legacy-output \
  data/hi1.png \
  data/hi8.png
```

The supplied 20-image dataset still produces the original legacy decision
summary:

```text
passed=18 failed=2
```

The original K-means initialization was nondeterministic, so some individual
rounded coverage values varied between runs. The current implementation resets
the OpenCV random seed for each clustering operation, making per-image results
repeatable and independent of batch order.

## Regression dataset

Normal mode currently produces:

| Status | Count |
|---|---:|
| Strict detections | 15 |
| Detections with warning | 2 |
| Not detected | 3 |
| Total | 20 |

The warning cases are `hi11` and `hi17`. The rejected cases are `hi6`, `hi7`,
and `hi13`.

These figures describe behavior on the supplied regression images. They are
not an accuracy benchmark and do not measure generalization to unseen images.

## Tests

The project builds the computer-vision pipeline as a reusable static library
and tests it at two levels:

- native C++ unit tests for geometry, coverage, invalid input, and status
  behavior;
- Python end-to-end tests for the CLI, dataset regression, determinism,
  legacy mode, and visualization generation.

Run all tests with:

```bash
ctest \
  --test-dir build \
  --output-on-failure
```

The same build and test suite runs automatically on Ubuntu through
GitHub Actions.

See [docs/TESTING.md](docs/TESTING.md) for details.

## Project structure

```text
.
├── CMakeLists.txt
├── include/                 Public C++ interfaces
├── src/                     Core pipeline, CLI and visualization
├── tests/                   C++ unit and Python CLI tests
├── data/                    Supplied regression images
├── docs/
│   ├── BASELINE.md
│   ├── DETECTION_MODEL.md
│   ├── TESTING.md
│   └── images/              README diagnostic examples
├── legacy/                  Original Visual Studio source files
└── .github/workflows/       Linux continuous integration
```

## Known limitations

- HSV thresholds are fixed and sensitive to lighting and color variation.
- False-positive patches influence PCA and K-means.
- All segmentation candidates participate in clustering.
- Final grid selection uses a greedy assignment rather than a global optimum.
- One incorrectly selected patch can distort spacing and the coverage hull.
- Coverage uses axis-aligned contour bounding boxes.
- Perspective distortion is not explicitly corrected with a homography.
- The expected color arrangement of the marker is not validated.
- Current spacing thresholds are heuristic.

The next algorithmic improvement should focus on candidate filtering and global
grid assignment rather than simply widening the spacing thresholds.
