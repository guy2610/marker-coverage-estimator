#pragma once

#include <opencv2/opencv.hpp>

#include <string>
#include <vector>

struct Patch {
    std::string color;
    cv::Rect box{};
    cv::Point2f center{};
    double area = 0.0;
    int id = -1;
};

enum class FailureReason {
    NONE = 0,
    FEW_PATCHES = 1,
    ASSIGN_GRID = 2,
    SPACING = 3,
    SMALL_HULL = 4,
    BAD_BBOX = 5,
    LOW_COVERAGE = 6,
    INVALID_IMAGE = 7
};

inline const char* fr_to_cstr(FailureReason reason) {
    switch (reason) {
    case FailureReason::NONE:
        return "OK";
    case FailureReason::FEW_PATCHES:
        return "FEW_PATCHES";
    case FailureReason::ASSIGN_GRID:
        return "GRID_ASSIGNMENT_FAILED";
    case FailureReason::SPACING:
        return "SPACING_VALIDATION_FAILED";
    case FailureReason::SMALL_HULL:
        return "HULL_TOO_SMALL";
    case FailureReason::BAD_BBOX:
        return "INVALID_BBOX";
    case FailureReason::LOW_COVERAGE:
        return "LOW_COVERAGE";
    case FailureReason::INVALID_IMAGE:
        return "INVALID_IMAGE";
    default:
        return "UNKNOWN";
    }
}

enum class DetectionStatus {
    NOT_DETECTED = 0,
    DETECTED = 1,
    DETECTED_WITH_WARNING = 2
};

inline const char* detection_status_to_cstr(
    DetectionStatus status
) {
    switch (status) {
    case DetectionStatus::NOT_DETECTED:
        return "NOT_DETECTED";
    case DetectionStatus::DETECTED:
        return "DETECTED";
    case DetectionStatus::DETECTED_WITH_WARNING:
        return "DETECTED_WITH_WARNING";
    default:
        return "UNKNOWN";
    }
}

struct GridDetection {
    // True when nine candidate patches were assigned to a 3x3 grid.
    bool ok = false;

    Patch grid[3][3]{};
    std::vector<cv::Point2f> rot;

    // Coefficients of variation for horizontal and vertical spacing.
    float cvx = 1e9F;
    float cvy = 1e9F;

    // Strict geometry validation.
    bool spacing_ok = false;

    // More tolerant geometry validation used to report a warning rather
    // than rejecting an otherwise plausible marker.
    bool spacing_soft_ok = false;
};
