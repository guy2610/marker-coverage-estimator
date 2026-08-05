#pragma once

#include "detection.hpp"

#include <opencv2/opencv.hpp>

// Produces an enlarged annotated image containing:
// - every segmentation candidate,
// - the selected 3x3 grid cells,
// - the coverage convex hull,
// - detection and geometry metrics.
cv::Mat render_detection_debug(
    const cv::Mat& image,
    const DetectionResult& result
);
