#pragma once

#include "types.hpp"

#include <opencv2/opencv.hpp>

#include <vector>

struct GridParams {
    // Strict thresholds.
    float cvx_thresh = 0.55F;
    float cvy_thresh = 0.65F;

    // Tolerant thresholds for a detection with a geometry warning.
    float cvx_soft_thresh = 0.70F;
    float cvy_soft_thresh = 0.70F;

    bool debug = false;
};

// Assigns candidate patches to a 3x3 grid and computes spacing metrics.
//
// FailureReason::NONE means that the grid and its spacing metrics were
// successfully computed. The caller should inspect spacing_ok and
// spacing_soft_ok to determine the detection quality.
FailureReason detect_grid_and_spacing(
    const std::vector<Patch>& patches,
    GridDetection& out,
    const GridParams& params,
    DetectionDiagnostics* diagnostics = nullptr
);
