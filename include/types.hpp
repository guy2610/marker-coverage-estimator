#pragma once

#include <opencv2/opencv.hpp>

#include <array>
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
    INVALID_IMAGE = 6
};

inline const char* fr_to_cstr(
    FailureReason reason
) {
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
    bool ok = false;

    Patch grid[3][3]{};
    std::vector<cv::Point2f> rot;

    float cvx = 1e9F;
    float cvy = 1e9F;

    bool spacing_ok = false;
    bool spacing_soft_ok = false;
};

// Optional read-only capture of intermediate values already computed by
// the detector. These diagnostics must never affect detection decisions.
struct SegmentationDiagnostics {
    std::array<std::string, 6> labels = {
        "red",
        "green",
        "blue",
        "yellow",
        "cyan",
        "magenta"
    };

    std::array<cv::Mat, 6> raw_masks;
    std::array<cv::Mat, 6> cleaned_masks;
};

struct GridDiagnostics {
    bool pca_available = false;
    cv::Point2f pca_mean{};
    std::array<cv::Point2f, 2> pca_axes{};

    bool clustering_available = false;
    std::vector<int> row_labels;
    std::vector<int> column_labels;
    std::array<float, 3> row_centers{};
    std::array<float, 3> column_centers{};

    std::array<std::array<int, 3>, 3> assigned_patch_ids = {{
        {{-1, -1, -1}},
        {{-1, -1, -1}},
        {{-1, -1, -1}}
    }};

    int assigned_count = 0;
};

struct DetectionDiagnostics {
    SegmentationDiagnostics segmentation;
    GridDiagnostics grid;
};
