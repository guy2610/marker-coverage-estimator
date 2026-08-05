#include "detection.hpp"

DetectionResult analyze_marker(
    const cv::Mat& image,
    const SegmentationParams& segmentation_params,
    const GridParams& grid_params,
    const LegacyDecisionParams& legacy_params
) {
    DetectionResult result;

    if (image.empty()) {
        result.failure_reason = FailureReason::INVALID_IMAGE;
        result.legacy_failure_reason = FailureReason::INVALID_IMAGE;
        return result;
    }

    result.patches =
        segment_color_patches(
            image,
            segmentation_params
        );

    const auto& patches = result.patches;

    result.patch_count = patches.size();

    if (patches.size() < 3) {
        result.failure_reason = FailureReason::FEW_PATCHES;
        result.legacy_failure_reason = FailureReason::FEW_PATCHES;
        return result;
    }

    const FailureReason grid_failure =
        detect_grid_and_spacing(
            patches,
            result.grid,
            grid_params
        );

    if (grid_failure != FailureReason::NONE) {
        result.failure_reason = grid_failure;
        result.legacy_failure_reason = grid_failure;
        return result;
    }

    if (!result.grid.ok) {
        result.failure_reason = FailureReason::ASSIGN_GRID;
        result.legacy_failure_reason = FailureReason::ASSIGN_GRID;
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
        result.failure_reason = FailureReason::BAD_BBOX;
        result.legacy_failure_reason = FailureReason::BAD_BBOX;
        return result;
    }

    result.coverage_available = true;

    // New decision:
    // Coverage describes marker size and does not determine marker
    // validity. Geometry is classified into strict, warning and failed
    // states.
    if (result.grid.spacing_ok) {
        result.status = DetectionStatus::DETECTED;
        result.failure_reason = FailureReason::NONE;
    } else if (result.grid.spacing_soft_ok) {
        result.status =
            DetectionStatus::DETECTED_WITH_WARNING;
        result.failure_reason = FailureReason::SPACING;
    } else {
        result.status = DetectionStatus::NOT_DETECTED;
        result.failure_reason = FailureReason::SPACING;
    }

    // Original assignment decision, retained for reproducibility.
    bool legacy_spacing_ok = result.grid.spacing_ok;

    if (
        !legacy_spacing_ok &&
        result.coverage.ratio >= legacy_params.coverage_fallback
    ) {
        legacy_spacing_ok = true;
    }

    if (
        !legacy_spacing_ok &&
        result.grid.cvx <= 0.60F &&
        result.grid.cvy <= 0.70F &&
        result.coverage.ratio >= legacy_params.coverage_soft
    ) {
        legacy_spacing_ok = true;
    }

    result.legacy_accepted =
        legacy_spacing_ok &&
        result.coverage.ratio >= legacy_params.coverage_threshold;

    // The original implementation labelled every final rejection after
    // coverage calculation as LOW_COVERAGE.
    result.legacy_failure_reason =
        result.legacy_accepted
            ? FailureReason::NONE
            : FailureReason::LOW_COVERAGE;

    return result;
}
