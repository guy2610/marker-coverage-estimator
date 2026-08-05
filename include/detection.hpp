#pragma once

#include "color_segmentation.hpp"
#include "coverage.hpp"
#include "grid_detector.hpp"
#include "types.hpp"

#include <opencv2/opencv.hpp>

#include <cstddef>

struct LegacyDecisionParams {
    // Thresholds retained solely for original-output regression.
    double coverage_threshold = 0.45;
    double coverage_fallback = 0.55;
    double coverage_soft = 0.50;
};

struct DetectionResult {
    std::size_t patch_count = 0;

    // All candidates returned by color segmentation. Retained for
    // diagnostics and visualization.
    std::vector<Patch> patches;

    DetectionStatus status =
        DetectionStatus::NOT_DETECTED;

    FailureReason failure_reason =
        FailureReason::NONE;

    // Original decision retained only for regression compatibility.
    bool legacy_accepted = false;

    FailureReason legacy_failure_reason =
        FailureReason::NONE;

    bool coverage_available = false;

    GridDetection grid;
    CoverageResult coverage;
};

DetectionResult analyze_marker(
    const cv::Mat& image,
    const SegmentationParams& segmentation_params,
    const GridParams& grid_params,
    const LegacyDecisionParams& legacy_params
);
