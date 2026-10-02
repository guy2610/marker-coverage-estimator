#pragma once

#include "detection.hpp"

#include <opencv2/opencv.hpp>

#include <filesystem>

// Produces an enlarged annotated image containing:
// - every segmentation candidate,
// - the selected 3x3 grid cells,
// - the coverage convex hull,
// - detection and geometry metrics.
cv::Mat render_detection_debug(
    const cv::Mat& image,
    const DetectionResult& result
);

// Saves a truthful sequence of detector-stage visualizations using only
// intermediate values captured from the real detection pipeline.
bool save_detection_stage_images(
    const std::filesystem::path& output_directory,
    const cv::Mat& image,
    const DetectionResult& result,
    const DetectionDiagnostics& diagnostics
);
