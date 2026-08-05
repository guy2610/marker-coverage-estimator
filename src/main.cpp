#include "color_segmentation.hpp"
#include "coverage.hpp"
#include "grid_detector.hpp"
#include "types.hpp"

#include <opencv2/opencv.hpp>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifndef MARKER_COVERAGE_VERSION
#define MARKER_COVERAGE_VERSION "development"
#endif

namespace {

using Clock = std::chrono::steady_clock;

void print_usage(const char* program_name) {
    std::cout
        << "Marker Coverage Estimator " << MARKER_COVERAGE_VERSION << "\n\n"
        << "Detect a 3x3 color marker and estimate the percentage of the\n"
        << "input image covered by its outer convex hull.\n\n"
        << "Usage:\n"
        << "  " << program_name << " [options] <image> [<image> ...]\n\n"
        << "Options:\n"
        << "  --debug       Print detection details and processing time\n"
        << "  -h, --help    Show this help message\n"
        << "  --version     Print the program version\n";
}

void emit_marker_result(
    const std::string& name,
    bool found,
    FailureReason failure_reason,
    bool debug_mode
) {
    if (!debug_mode) {
        return;
    }

    if (found) {
        std::cout << "marker_found " << name << "\n";
    } else {
        std::cout
            << "marker_not_found "
            << name << " "
            << fr_to_cstr(failure_reason)
            << "\n";
    }
}

}  // namespace

int main(int argc, char** argv) {
    bool debug_mode = false;
    bool invalid_input = false;
    std::vector<std::string> image_paths;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];

        if (argument == "-h" || argument == "--help") {
            print_usage(argv[0]);
            return 0;
        }

        if (argument == "--version") {
            std::cout << MARKER_COVERAGE_VERSION << "\n";
            return 0;
        }

        if (argument == "--debug") {
            debug_mode = true;
            continue;
        }

        if (!argument.empty() && argument.front() == '-') {
            std::cerr << "Unknown option: " << argument << "\n\n";
            print_usage(argv[0]);
            return 2;
        }

        if (!std::filesystem::is_regular_file(argument)) {
            std::cerr << "Invalid image path: " << argument << "\n";
            invalid_input = true;
            continue;
        }

        image_paths.push_back(argument);
    }

    if (argc < 2 || image_paths.empty()) {
        if (argc < 2) {
            print_usage(argv[0]);
        } else if (invalid_input) {
            std::cerr << "No valid image files were provided.\n";
        }

        return 2;
    }

    int pass_count = 0;
    int fail_count = 0;
    bool any_detection_failed = false;

    SegmentationParams segmentation_params;
    segmentation_params.debug = debug_mode;

    GridParams grid_params;
    grid_params.debug = debug_mode;

    // Baseline thresholds retained for reproducibility.
    grid_params.coverage_thresh = 0.45F;
    grid_params.coverage_fallback = 0.55F;
    grid_params.coverage_soft = 0.50F;

    for (const auto& path : image_paths) {
        const auto start_time = Clock::now();

        const cv::Mat image = cv::imread(path, cv::IMREAD_COLOR);
        if (image.empty()) {
            emit_marker_result(
                path,
                false,
                FailureReason::FEW_PATCHES,
                debug_mode
            );

            if (!debug_mode) {
                std::cout << path << " 0%\n";
            }

            ++fail_count;
            any_detection_failed = true;
            continue;
        }

        const auto patches =
            segment_color_patches(image, segmentation_params);

        if (patches.size() < 3) {
            emit_marker_result(
                path,
                false,
                FailureReason::FEW_PATCHES,
                debug_mode
            );

            if (!debug_mode) {
                std::cout << path << " 0%\n";
            }

            ++fail_count;
            any_detection_failed = true;
            continue;
        }

        GridDetection grid_detection;
        const FailureReason grid_failure =
            detect_grid_and_spacing(
                patches,
                grid_detection,
                grid_params
            );

        if (
            grid_failure == FailureReason::ASSIGN_GRID ||
            grid_failure == FailureReason::SPACING
        ) {
            emit_marker_result(
                path,
                false,
                grid_failure,
                debug_mode
            );

            if (!debug_mode) {
                std::cout << path << " 0%\n";
            }

            ++fail_count;
            any_detection_failed = true;

            if (debug_mode) {
                const auto elapsed =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        Clock::now() - start_time
                    ).count();

                std::cout << path << " took " << elapsed << " ms\n";
            }

            continue;
        }

        const auto coverage =
            compute_coverage_from_grid(
                grid_detection.grid,
                image.size()
            );

        if (
            coverage.hull_area <= 0.0 ||
            coverage.image_area <= 0.0
        ) {
            emit_marker_result(
                path,
                false,
                FailureReason::BAD_BBOX,
                debug_mode
            );

            if (!debug_mode) {
                std::cout << path << " 0%\n";
            }

            ++fail_count;
            any_detection_failed = true;
            continue;
        }

        bool spacing_ok = grid_detection.spacing_ok;

        if (
            !spacing_ok &&
            coverage.ratio >= grid_params.coverage_fallback
        ) {
            spacing_ok = true;
        }

        if (
            !spacing_ok &&
            grid_detection.cvx <= 0.60F &&
            grid_detection.cvy <= 0.70F &&
            coverage.ratio >= grid_params.coverage_soft
        ) {
            spacing_ok = true;
        }

        const bool marker_found =
            spacing_ok &&
            coverage.ratio >= grid_params.coverage_thresh;

        emit_marker_result(
            path,
            marker_found,
            marker_found
                ? FailureReason::NONE
                : FailureReason::LOW_COVERAGE,
            debug_mode
        );

        if (marker_found) {
            ++pass_count;

            const int percentage =
                static_cast<int>(
                    std::lround(coverage.ratio * 100.0)
                );

            if (!debug_mode) {
                std::cout
                    << path << " "
                    << percentage << "%\n";
            } else {
                std::cout
                    << "[coverage] hull="
                    << coverage.hull_area
                    << " image="
                    << coverage.image_area
                    << " ratio="
                    << coverage.ratio
                    << " ("
                    << percentage
                    << "%)\n";
            }
        } else {
            ++fail_count;
            any_detection_failed = true;

            if (!debug_mode) {
                std::cout << path << " 0%\n";
            } else {
                std::cout
                    << "[final] cvx="
                    << grid_detection.cvx
                    << " cvy="
                    << grid_detection.cvy
                    << " coverage_ratio="
                    << coverage.ratio
                    << "\n";
            }
        }

        if (debug_mode) {
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    Clock::now() - start_time
                ).count();

            if (elapsed > 200) {
                std::cerr
                    << "[warn] "
                    << path
                    << " took "
                    << elapsed
                    << " ms (>200ms)\n";
            } else {
                std::cout
                    << path
                    << " took "
                    << elapsed
                    << " ms\n";
            }
        }
    }

    std::cout
        << "\nSummary: passed="
        << pass_count
        << " failed="
        << fail_count
        << " out of "
        << pass_count + fail_count
        << "\n";

    if (invalid_input) {
        return 2;
    }

    return any_detection_failed ? 1 : 0;
}
