# Testing

Marker Coverage Estimator uses CTest with two test layers:

1. Native C++ core tests.
2. Python end-to-end tests using generated synthetic images.

No private third-party images are required by the public build or test suite.

## Configure

```bash
cmake \
  -S . \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON
```

A custom `OpenCV_DIR` may be supplied when OpenCV is installed in a
non-standard location.

## Build and run

```bash
cmake --build build --parallel

ctest \
  --test-dir build \
  --output-on-failure
```

## Core unit tests

The C++ tests validate:

- convex-hull coverage for a regular synthetic grid;
- strict geometry validation for a perfect 3×3 grid;
- invalid-image handling;
- an image without color patches;
- public status and failure strings.

These tests call the core C++ API directly.

## Synthetic CLI regression

The end-to-end test:

1. Runs `generate-synthetic-dataset` in a temporary directory.
2. Verifies the generated images and manifest.
3. Runs the compiled detector against stable synthetic cases.
4. Checks positive, warning and negative classifications.
5. Checks low-coverage marker reporting.
6. Verifies repeated-run determinism.
7. Verifies batch-order independence.
8. Verifies annotated visualization generation.

The stable synthetic summary is currently:

```text
detected=5 warnings=1 not_detected=2 out of 8
```

The generator also creates blur/noise and colored-distractor challenge scenes.
Their current failure is documented, but it is not locked into the required
regression behavior.

## Synthetic generation

Generate the public scenes manually with:

```bash
./build/generate-synthetic-dataset \
  output/synthetic
```

The output contains:

- `images/`
- `manifest.csv`
- `contact_sheet.png`

The manifest records intended scenario labels and polygon-based coverage
ground truth.
