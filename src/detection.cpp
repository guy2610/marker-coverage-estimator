#include "detection.hpp"

DetectionResult analyze_marker(
    const cv::Mat& image,
    const SegmentationParams& segmentation_params,
    const GridParams& grid_params,
    DetectionDiagnostics* diagnostics
) {
    DetectionResult result;

    if (diagnostics != nullptr) {
        *diagnostics =
            DetectionDiagnostics{};
    }

    if (image.empty()) {
        result.failure_reason =
            FailureReason::INVALID_IMAGE;

        return result;
    }

    result.patches =
        segment_color_patches(
            image,
            segmentation_params,
            diagnostics
        );

    const auto& patches =
        result.patches;

    result.patch_count =
        patches.size();

    if (patches.size() < 3) {
        result.failure_reason =
            FailureReason::FEW_PATCHES;

        return result;
    }

    const FailureReason grid_failure =
        detect_grid_and_spacing(
            patches,
            result.grid,
            grid_params,
            diagnostics
        );

    if (grid_failure != FailureReason::NONE) {
        result.failure_reason =
            grid_failure;

        return result;
    }

    if (!result.grid.ok) {
        result.failure_reason =
            FailureReason::ASSIGN_GRID;

        return result;
    }

    result.coverage =
        compute_coverage_from_grid(
            result.grid.grid,
            image.size()
        );

    if (
        result.coverage.hull_area <= 0.0 ||
        result.coverage.image_area <= 0.0
    ) {
        result.failure_reason =
            FailureReason::BAD_BBOX;

        return result;
    }

    result.coverage_available = true;

    if (result.grid.spacing_ok) {
        result.status =
            DetectionStatus::DETECTED;

        result.failure_reason =
            FailureReason::NONE;
    } else if (result.grid.spacing_soft_ok) {
        result.status =
            DetectionStatus::DETECTED_WITH_WARNING;

        result.failure_reason =
            FailureReason::SPACING;
    } else {
        result.status =
            DetectionStatus::NOT_DETECTED;

        result.failure_reason =
            FailureReason::SPACING;
    }

    return result;
}
