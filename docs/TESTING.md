# Testing

Marker Coverage Estimator uses CTest as its test runner.

The test suite contains two layers:

1. C++ core unit tests.
2. End-to-end CLI and dataset regression tests.

## Configure

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON
```

A custom `OpenCV_DIR` may be supplied when OpenCV is installed in a
non-standard location.

## Build and run

```bash
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Core unit tests

The native C++ tests validate:

- convex-hull coverage for a synthetic regular grid;
- strict geometry validation for a perfect synthetic 3×3 grid;
- invalid-image handling;
- an image containing no color patches;
- public status and failure strings.

These tests call the core C++ API directly.

## CLI regression tests

The Python end-to-end test invokes the compiled executable and validates:

- help, version and invalid-option behavior;
- low-coverage marker detection using `hi8`;
- spacing rejection using `hi6`;
- warning classification using `hi11`;
- the current deterministic 20-image result;
- identical repeated runs;
- independence from batch processing order;
- the legacy 18/20 acceptance decisions;
- annotated visualization generation.

The sample-image expectations are regression tests for the supplied dataset.
They do not by themselves measure performance on unseen images.
