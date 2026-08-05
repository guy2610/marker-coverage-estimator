#include "coverage.hpp"
#include "detection.hpp"
#include "grid_detector.hpp"
#include "types.hpp"

#include <opencv2/opencv.hpp>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void fail(const std::string& message) {
    std::cerr
        << "FAILED: "
        << message
        << "\n";

    std::exit(EXIT_FAILURE);
}

void expect_true(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        fail(message);
    }
}

void expect_near(
    double actual,
    double expected,
    double tolerance,
    const std::string& message
) {
    if (std::abs(actual - expected) > tolerance) {
        std::cerr
            << "Expected: "
            << expected
            << "\nActual:   "
            << actual
            << "\n";

        fail(message);
    }
}

Patch make_patch(
    int id,
    int x,
    int y,
    int width,
    int height
) {
    const cv::Rect box(
        x,
        y,
        width,
        height
    );

    return Patch{
        "synthetic",
        box,
        cv::Point2f(
            static_cast<float>(x) +
                static_cast<float>(width) / 2.0F,
            static_cast<float>(y) +
                static_cast<float>(height) / 2.0F
        ),
        static_cast<double>(width * height),
        id
    };
}

void test_coverage_for_regular_grid() {
    Patch grid[3][3];

    int id = 0;

    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            grid[row][column] =
                make_patch(
                    id++,
                    column * 10,
                    row * 10,
                    10,
                    10
                );
        }
    }

    const CoverageResult coverage =
        compute_coverage_from_grid(
            grid,
            cv::Size(100, 100)
        );

    expect_near(
        coverage.hull_area,
        900.0,
        1e-6,
        "Regular grid hull area"
    );

    expect_near(
        coverage.bbox_area,
        900.0,
        1e-6,
        "Regular grid bounding-box area"
    );

    expect_near(
        coverage.ratio_bbox,
        1.0,
        1e-6,
        "Hull should fill its bounding box"
    );

    expect_near(
        coverage.ratio,
        0.09,
        1e-6,
        "Coverage should equal 9 percent"
    );

    expect_true(
        coverage.hull.size() == 4,
        "Regular rectangular grid should have a four-point hull"
    );
}

void test_perfect_grid_geometry() {
    std::vector<Patch> patches;
    patches.reserve(9);

    int id = 0;

    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            patches.push_back(
                make_patch(
                    id++,
                    12 + column * 24,
                    10 + row * 18,
                    8,
                    8
                )
            );
        }
    }

    GridDetection detection;
    const GridParams parameters;

    const FailureReason reason =
        detect_grid_and_spacing(
            patches,
            detection,
            parameters
        );

    expect_true(
        reason == FailureReason::NONE,
        "Perfect grid should produce spacing metrics"
    );

    expect_true(
        detection.ok,
        "Perfect grid should assign all nine cells"
    );

    expect_true(
        detection.spacing_ok,
        "Perfect grid should pass strict spacing"
    );

    expect_true(
        detection.spacing_soft_ok,
        "Perfect grid should pass tolerant spacing"
    );

    expect_near(
        detection.cvx,
        0.0,
        1e-5,
        "Perfect horizontal spacing variation"
    );

    expect_near(
        detection.cvy,
        0.0,
        1e-5,
        "Perfect vertical spacing variation"
    );
}

void test_invalid_image() {
    const cv::Mat empty_image;

    const DetectionResult result =
        analyze_marker(
            empty_image,
            SegmentationParams{},
            GridParams{},
            LegacyDecisionParams{}
        );

    expect_true(
        result.status ==
            DetectionStatus::NOT_DETECTED,
        "Empty image should not be detected"
    );

    expect_true(
        result.failure_reason ==
            FailureReason::INVALID_IMAGE,
        "Empty image should report INVALID_IMAGE"
    );

    expect_true(
        !result.coverage_available,
        "Empty image should have no coverage"
    );

    expect_true(
        result.patch_count == 0,
        "Empty image should have no patches"
    );
}

void test_image_without_color_patches() {
    const cv::Mat black_image =
        cv::Mat::zeros(
            100,
            100,
            CV_8UC3
        );

    const DetectionResult result =
        analyze_marker(
            black_image,
            SegmentationParams{},
            GridParams{},
            LegacyDecisionParams{}
        );

    expect_true(
        result.status ==
            DetectionStatus::NOT_DETECTED,
        "Black image should not contain a marker"
    );

    expect_true(
        result.failure_reason ==
            FailureReason::FEW_PATCHES,
        "Black image should report too few patches"
    );

    expect_true(
        result.patch_count == 0,
        "Black image should produce zero color patches"
    );
}

void test_status_strings() {
    expect_true(
        std::string(
            detection_status_to_cstr(
                DetectionStatus::DETECTED
            )
        ) == "DETECTED",
        "DETECTED status text"
    );

    expect_true(
        std::string(
            detection_status_to_cstr(
                DetectionStatus::DETECTED_WITH_WARNING
            )
        ) == "DETECTED_WITH_WARNING",
        "Warning status text"
    );

    expect_true(
        std::string(
            fr_to_cstr(
                FailureReason::SPACING
            )
        ) == "SPACING_VALIDATION_FAILED",
        "Spacing failure text"
    );
}

}  // namespace

int main() {
    test_coverage_for_regular_grid();
    test_perfect_grid_geometry();
    test_invalid_image();
    test_image_without_color_patches();
    test_status_strings();

    std::cout
        << "All core unit tests passed.\n";

    return EXIT_SUCCESS;
}
