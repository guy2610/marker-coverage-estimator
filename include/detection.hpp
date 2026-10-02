#pragma once

#include "color_segmentation.hpp"
#include "coverage.hpp"
#include "grid_detector.hpp"
#include "types.hpp"

#include <opencv2/opencv.hpp>

#include <cstddef>
#include <vector>

struct DetectionResult {
    std::size_t patch_count = 0;

    // Retained for diagnostics and visualization.
    std::vector<Patch> patches;

    DetectionStatus status =
        DetectionStatus::NOT_DETECTED;

    FailureReason failure_reason =
        FailureReason::NONE;

    bool coverage_available = false;

    GridDetection grid;
    CoverageResult coverage;
};

DetectionResult analyze_marker(
    const cv::Mat& image,
    const SegmentationParams& segmentation_params,
    const GridParams& grid_params,
    DetectionDiagnostics* diagnostics = nullptr
);
