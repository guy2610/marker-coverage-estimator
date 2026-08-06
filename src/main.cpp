#include "detection.hpp"
#include "types.hpp"
#include "visualization.hpp"

#include <opencv2/opencv.hpp>

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef MARKER_COVERAGE_VERSION
#define MARKER_COVERAGE_VERSION "development"
#endif

namespace {

using Clock = std::chrono::steady_clock;

void print_usage(const char* program_name) {
    std::cout
        << "Marker Coverage Estimator "
        << MARKER_COVERAGE_VERSION
        << "\n\n"
        << "Detect a 3x3 color marker and estimate the percentage of the\n"
        << "input image covered by its outer convex hull.\n\n"
        << "Usage:\n"
        << "  "
        << program_name
        << " [options] <image> [<image> ...]\n\n"
        << "Options:\n"
        << "  --debug          Print intermediate detection metrics\n"
        << "  --visualize-dir DIR\n"
        << "                    Save annotated debug images to DIR\n"
        << "  -h, --help       Show this help message\n"
        << "  --version        Print the program version\n";
}

std::string format_percentage(
    double ratio,
    int decimal_places
) {
    std::ostringstream output;

    output
        << std::fixed
        << std::setprecision(decimal_places)
        << ratio * 100.0
        << "%";

    return output.str();
}

void print_debug_result(
    const std::string& path,
    const DetectionResult& result,
    long long elapsed_microseconds
) {
    std::cout
        << "[analysis] image="
        << path
        << " patches="
        << result.patch_count
        << " grid_assigned="
        << (result.grid.ok ? "yes" : "no")
        << " spacing_strict="
        << (result.grid.spacing_ok ? "yes" : "no")
        << " spacing_soft="
        << (result.grid.spacing_soft_ok ? "yes" : "no")
        << " status="
        << detection_status_to_cstr(result.status);

    if (result.grid.ok) {
        std::cout
            << " cvx="
            << result.grid.cvx
            << " cvy="
            << result.grid.cvy;
    }

    if (result.coverage_available) {
        std::cout
            << " coverage="
            << format_percentage(
                result.coverage.ratio,
                2
            )
            << " hull_area="
            << result.coverage.hull_area
            << " image_area="
            << result.coverage.image_area;
    }

    std::cout
        << " elapsed_us="
        << elapsed_microseconds
        << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    bool debug_mode = false;
    bool invalid_input = false;

    std::filesystem::path visualization_directory;
    std::vector<std::string> image_paths;

    for (int index = 1; index < argc; ++index) {
        const std::string argument =
            argv[index];

        if (
            argument == "-h" ||
            argument == "--help"
        ) {
            print_usage(argv[0]);
            return 0;
        }

        if (argument == "--version") {
            std::cout
                << MARKER_COVERAGE_VERSION
                << "\n";

            return 0;
        }

        if (argument == "--debug") {
            debug_mode = true;
            continue;
        }

        if (argument == "--visualize-dir") {
            if (index + 1 >= argc) {
                std::cerr
                    << "--visualize-dir requires "
                    << "a directory path.\n";

                return 2;
            }

            visualization_directory =
                argv[++index];

            continue;
        }

        if (
            !argument.empty() &&
            argument.front() == '-'
        ) {
            std::cerr
                << "Unknown option: "
                << argument
                << "\n\n";

            print_usage(argv[0]);
            return 2;
        }

        if (
            !std::filesystem::is_regular_file(
                argument
            )
        ) {
            std::cerr
                << "Invalid image path: "
                << argument
                << "\n";

            invalid_input = true;
            continue;
        }

        image_paths.push_back(argument);
    }

    if (argc < 2 || image_paths.empty()) {
        if (argc < 2) {
            print_usage(argv[0]);
        } else {
            std::cerr
                << "No valid image files were provided.\n";
        }

        return 2;
    }

    if (!visualization_directory.empty()) {
        std::error_code directory_error;

        std::filesystem::create_directories(
            visualization_directory,
            directory_error
        );

        if (directory_error) {
            std::cerr
                << "Could not create visualization directory: "
                << visualization_directory
                << "\n";

            return 2;
        }
    }

    SegmentationParams segmentation_params;
    segmentation_params.debug = debug_mode;

    GridParams grid_params;
    grid_params.debug = debug_mode;

    int detected_count = 0;
    int warning_count = 0;
    int not_detected_count = 0;

    bool processing_error = false;

    for (const auto& path : image_paths) {
        const auto start_time =
            Clock::now();

        const cv::Mat image =
            cv::imread(
                path,
                cv::IMREAD_COLOR
            );

        const DetectionResult result =
            analyze_marker(
                image,
                segmentation_params,
                grid_params
            );

        const auto elapsed_microseconds =
            std::chrono::duration_cast<
                std::chrono::microseconds
            >(
                Clock::now() - start_time
            ).count();

        if (
            result.failure_reason ==
            FailureReason::INVALID_IMAGE
        ) {
            processing_error = true;
        }

        if (
            result.status ==
            DetectionStatus::DETECTED
        ) {
            std::cout
                << path
                << " marker_found coverage="
                << format_percentage(
                    result.coverage.ratio,
                    1
                )
                << "\n";

            ++detected_count;
        } else if (
            result.status ==
            DetectionStatus::DETECTED_WITH_WARNING
        ) {
            std::cout
                << path
                << " marker_found coverage="
                << format_percentage(
                    result.coverage.ratio,
                    1
                )
                << " warning=IRREGULAR_SPACING\n";

            ++warning_count;
        } else {
            std::cout
                << path
                << " marker_not_found reason="
                << fr_to_cstr(
                    result.failure_reason
                )
                << "\n";

            ++not_detected_count;
        }

        if (debug_mode) {
            print_debug_result(
                path,
                result,
                elapsed_microseconds
            );
        }

        if (
            !visualization_directory.empty() &&
            !image.empty()
        ) {
            const cv::Mat visualization =
                render_detection_debug(
                    image,
                    result
                );

            const std::filesystem::path input_path(
                path
            );

            const std::filesystem::path output_path =
                visualization_directory /
                (
                    input_path.stem().string() +
                    "_annotated.png"
                );

            try {
                if (
                    visualization.empty() ||
                    !cv::imwrite(
                        output_path.string(),
                        visualization
                    )
                ) {
                    std::cerr
                        << "Could not save visualization: "
                        << output_path
                        << "\n";

                    processing_error = true;
                } else if (debug_mode) {
                    std::cout
                        << "[visualization] "
                        << output_path
                        << "\n";
                }
            } catch (
                const cv::Exception& exception
            ) {
                std::cerr
                    << "Could not save visualization: "
                    << output_path
                    << " ("
                    << exception.what()
                    << ")\n";

                processing_error = true;
            }
        }
    }

    std::cout
        << "\nSummary: detected="
        << detected_count
        << " warnings="
        << warning_count
        << " not_detected="
        << not_detected_count
        << " out of "
        << (
            detected_count +
            warning_count +
            not_detected_count
        )
        << "\n";

    if (invalid_input) {
        return 2;
    }

    if (processing_error) {
        return 3;
    }

    // A valid image without a marker is a successful analysis result.
    return 0;
}
