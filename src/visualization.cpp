#include "visualization.hpp"

#include "types.hpp"

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>

namespace {

constexpr int header_height = 92;

cv::Point scale_point(
    const cv::Point2f& point,
    double scale
) {
    return cv::Point(
        cvRound(point.x * scale),
        cvRound(point.y * scale) + header_height
    );
}

cv::Rect scale_rectangle(
    const cv::Rect& rectangle,
    double scale
) {
    return cv::Rect(
        cvRound(rectangle.x * scale),
        cvRound(rectangle.y * scale) + header_height,
        std::max(
            1,
            cvRound(rectangle.width * scale)
        ),
        std::max(
            1,
            cvRound(rectangle.height * scale)
        )
    );
}

cv::Scalar status_color(
    DetectionStatus status
) {
    switch (status) {
    case DetectionStatus::DETECTED:
        return cv::Scalar(80, 220, 80);

    case DetectionStatus::DETECTED_WITH_WARNING:
        return cv::Scalar(0, 190, 255);

    case DetectionStatus::NOT_DETECTED:
    default:
        return cv::Scalar(70, 70, 240);
    }
}

std::string format_ratio(double ratio) {
    std::ostringstream output;

    output
        << std::fixed
        << std::setprecision(2)
        << ratio * 100.0
        << "%";

    return output.str();
}

}  // namespace

cv::Mat render_detection_debug(
    const cv::Mat& image,
    const DetectionResult& result
) {
    if (image.empty()) {
        return {};
    }

    const double scale = std::clamp(
        900.0 /
            static_cast<double>(image.cols),
        1.0,
        12.0
    );

    cv::Mat enlarged;

    cv::resize(
        image,
        enlarged,
        cv::Size(),
        scale,
        scale,
        cv::INTER_CUBIC
    );

    cv::Mat canvas;

    cv::copyMakeBorder(
        enlarged,
        canvas,
        header_height,
        0,
        0,
        0,
        cv::BORDER_CONSTANT,
        cv::Scalar(24, 24, 24)
    );

    const cv::Scalar candidate_color(
        0,
        220,
        255
    );

    // Draw every patch returned by segmentation.
    for (const Patch& patch : result.patches) {
        const cv::Rect rectangle =
            scale_rectangle(
                patch.box,
                scale
            );

        cv::rectangle(
            canvas,
            rectangle,
            candidate_color,
            2,
            cv::LINE_AA
        );

        const std::string label =
            std::to_string(patch.id) +
            ":" +
            patch.color;

        cv::putText(
            canvas,
            label,
            cv::Point(
                rectangle.x,
                std::max(
                    header_height + 14,
                    rectangle.y - 5
                )
            ),
            cv::FONT_HERSHEY_SIMPLEX,
            0.48,
            candidate_color,
            1,
            cv::LINE_AA
        );
    }

    const std::array<cv::Scalar, 3> row_colors = {
        cv::Scalar(80, 220, 80),
        cv::Scalar(255, 170, 40),
        cv::Scalar(220, 80, 220)
    };

    // Draw the nine patches selected for the grid.
    if (result.grid.ok) {
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                const Patch& patch =
                    result.grid.grid[row][column];

                const cv::Rect rectangle =
                    scale_rectangle(
                        patch.box,
                        scale
                    );

                cv::rectangle(
                    canvas,
                    rectangle,
                    row_colors[row],
                    4,
                    cv::LINE_AA
                );

                const std::string cell_label =
                    "r" +
                    std::to_string(row) +
                    "c" +
                    std::to_string(column) +
                    " #" +
                    std::to_string(patch.id);

                cv::putText(
                    canvas,
                    cell_label,
                    cv::Point(
                        rectangle.x,
                        rectangle.y +
                            std::min(
                                rectangle.height - 4,
                                22
                            )
                    ),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.58,
                    row_colors[row],
                    2,
                    cv::LINE_AA
                );
            }
        }
    }

    // Draw the outer hull used for coverage calculation.
    if (
        result.coverage_available &&
        result.coverage.hull.size() >= 3
    ) {
        std::vector<cv::Point> scaled_hull;
        scaled_hull.reserve(
            result.coverage.hull.size()
        );

        for (const auto& point : result.coverage.hull) {
            scaled_hull.push_back(
                scale_point(
                    point,
                    scale
                )
            );
        }

        const std::vector<std::vector<cv::Point>>
            hulls = {scaled_hull};

        cv::polylines(
            canvas,
            hulls,
            true,
            cv::Scalar(255, 80, 220),
            4,
            cv::LINE_AA
        );
    }

    const cv::Scalar state_color =
        status_color(result.status);

    cv::putText(
        canvas,
        detection_status_to_cstr(result.status),
        cv::Point(18, 30),
        cv::FONT_HERSHEY_SIMPLEX,
        0.78,
        state_color,
        2,
        cv::LINE_AA
    );

    std::ostringstream metrics;

    metrics
        << "patches="
        << result.patch_count
        << "  grid="
        << (result.grid.ok ? "yes" : "no");

    if (result.grid.ok) {
        metrics
            << "  cvx="
            << std::fixed
            << std::setprecision(3)
            << result.grid.cvx
            << "  cvy="
            << result.grid.cvy;
    }

    if (result.coverage_available) {
        metrics
            << "  coverage="
            << format_ratio(
                result.coverage.ratio
            );
    }

    cv::putText(
        canvas,
        metrics.str(),
        cv::Point(18, 64),
        cv::FONT_HERSHEY_SIMPLEX,
        0.62,
        cv::Scalar(235, 235, 235),
        1,
        cv::LINE_AA
    );

    cv::putText(
        canvas,
        "yellow=candidates  colored=selected grid  pink=coverage hull",
        cv::Point(18, 86),
        cv::FONT_HERSHEY_SIMPLEX,
        0.46,
        cv::Scalar(180, 180, 180),
        1,
        cv::LINE_AA
    );

    return canvas;
}
namespace {

cv::Mat make_stage_canvas(
    const cv::Mat& image,
    const std::string& title
) {
    if (image.empty()) {
        return {};
    }

    const double scale = std::clamp(
        900.0 /
            static_cast<double>(image.cols),
        1.0,
        12.0
    );

    cv::Mat enlarged;

    cv::resize(
        image,
        enlarged,
        cv::Size(),
        scale,
        scale,
        cv::INTER_CUBIC
    );

    cv::Mat canvas;

    cv::copyMakeBorder(
        enlarged,
        canvas,
        header_height,
        0,
        0,
        0,
        cv::BORDER_CONSTANT,
        cv::Scalar(24, 24, 24)
    );

    cv::putText(
        canvas,
        title,
        cv::Point(18, 40),
        cv::FONT_HERSHEY_SIMPLEX,
        0.82,
        cv::Scalar(235, 235, 235),
        2,
        cv::LINE_AA
    );

    return canvas;
}

cv::Mat render_mask_sheet(
    const std::array<cv::Mat, 6>& masks,
    const std::array<std::string, 6>& labels,
    const std::string& title
) {
    if (masks[0].empty()) {
        return {};
    }

    constexpr int columns = 3;
    constexpr int rows = 2;
    constexpr int tile_width = 300;
    constexpr int label_height = 34;
    constexpr int title_height = 54;

    const int image_height = std::max(
        1,
        cvRound(
            static_cast<double>(masks[0].rows) *
            static_cast<double>(tile_width) /
            static_cast<double>(masks[0].cols)
        )
    );

    const int tile_height =
        label_height +
        image_height;

    cv::Mat sheet(
        title_height + rows * tile_height,
        columns * tile_width,
        CV_8UC3,
        cv::Scalar(24, 24, 24)
    );

    cv::putText(
        sheet,
        title,
        cv::Point(18, 36),
        cv::FONT_HERSHEY_SIMPLEX,
        0.82,
        cv::Scalar(235, 235, 235),
        2,
        cv::LINE_AA
    );

    for (std::size_t index = 0; index < masks.size(); ++index) {
        if (masks[index].empty()) {
            continue;
        }

        const int row =
            static_cast<int>(index) /
            columns;

        const int column =
            static_cast<int>(index) %
            columns;

        cv::Mat resized_mask;

        cv::resize(
            masks[index],
            resized_mask,
            cv::Size(
                tile_width,
                image_height
            ),
            0.0,
            0.0,
            cv::INTER_NEAREST
        );

        cv::Mat colored_mask;

        cv::cvtColor(
            resized_mask,
            colored_mask,
            cv::COLOR_GRAY2BGR
        );

        const cv::Rect destination(
            column * tile_width,
            title_height +
                row * tile_height +
                label_height,
            tile_width,
            image_height
        );

        colored_mask.copyTo(
            sheet(destination)
        );

        cv::putText(
            sheet,
            labels[index],
            cv::Point(
                column * tile_width + 12,
                title_height +
                    row * tile_height +
                    24
            ),
            cv::FONT_HERSHEY_SIMPLEX,
            0.62,
            cv::Scalar(220, 220, 220),
            1,
            cv::LINE_AA
        );
    }

    return sheet;
}

cv::Mat render_candidates_stage(
    const cv::Mat& image,
    const DetectionResult& result
) {
    cv::Mat canvas =
        make_stage_canvas(
            image,
            "Accepted segmentation candidates"
        );

    if (canvas.empty()) {
        return {};
    }

    const double scale = std::clamp(
        900.0 /
            static_cast<double>(image.cols),
        1.0,
        12.0
    );

    const cv::Scalar candidate_color(
        0,
        220,
        255
    );

    for (const Patch& patch : result.patches) {
        const cv::Rect rectangle =
            scale_rectangle(
                patch.box,
                scale
            );

        const cv::Point center =
            scale_point(
                patch.center,
                scale
            );

        cv::rectangle(
            canvas,
            rectangle,
            candidate_color,
            2,
            cv::LINE_AA
        );

        cv::circle(
            canvas,
            center,
            5,
            cv::Scalar(255, 255, 255),
            -1,
            cv::LINE_AA
        );

        const std::string label =
            "#" +
            std::to_string(patch.id) +
            " " +
            patch.color;

        cv::putText(
            canvas,
            label,
            cv::Point(
                rectangle.x,
                std::max(
                    header_height + 16,
                    rectangle.y - 5
                )
            ),
            cv::FONT_HERSHEY_SIMPLEX,
            0.48,
            candidate_color,
            1,
            cv::LINE_AA
        );
    }

    return canvas;
}

cv::Mat render_geometry_stage(
    const cv::Mat& image,
    const DetectionResult& result,
    const DetectionDiagnostics& diagnostics
) {
    cv::Mat canvas =
        make_stage_canvas(
            image,
            "PCA orientation and row/column clustering"
        );

    if (canvas.empty()) {
        return {};
    }

    const double scale = std::clamp(
        900.0 /
            static_cast<double>(image.cols),
        1.0,
        12.0
    );

    for (
        std::size_t index = 0;
        index < result.patches.size();
        ++index
    ) {
        const Patch& patch =
            result.patches[index];

        const cv::Point center =
            scale_point(
                patch.center,
                scale
            );

        cv::circle(
            canvas,
            center,
            6,
            cv::Scalar(0, 220, 255),
            -1,
            cv::LINE_AA
        );

        std::string label =
            "#" +
            std::to_string(patch.id);

        if (
            diagnostics.grid.clustering_available &&
            index <
                diagnostics.grid.row_labels.size() &&
            index <
                diagnostics.grid.column_labels.size()
        ) {
            label +=
                " r" +
                std::to_string(
                    diagnostics.grid.row_labels[index]
                ) +
                " c" +
                std::to_string(
                    diagnostics
                        .grid
                        .column_labels[index]
                );
        }

        cv::putText(
            canvas,
            label,
            center + cv::Point(8, -8),
            cv::FONT_HERSHEY_SIMPLEX,
            0.46,
            cv::Scalar(245, 245, 245),
            1,
            cv::LINE_AA
        );
    }

    if (diagnostics.grid.pca_available) {
        const cv::Point2f mean =
            diagnostics.grid.pca_mean;

        const float axis_length =
            0.22F *
            static_cast<float>(
                std::min(
                    image.cols,
                    image.rows
                )
            );

        const std::array<cv::Scalar, 2>
            axis_colors = {
                cv::Scalar(80, 220, 80),
                cv::Scalar(255, 170, 40)
            };

        for (int axis = 0; axis < 2; ++axis) {
            const cv::Point2f direction =
                diagnostics
                    .grid
                    .pca_axes[axis];

            const cv::Point2f start =
                mean -
                direction *
                    axis_length;

            const cv::Point2f end =
                mean +
                direction *
                    axis_length;

            cv::line(
                canvas,
                scale_point(
                    start,
                    scale
                ),
                scale_point(
                    end,
                    scale
                ),
                axis_colors[axis],
                3,
                cv::LINE_AA
            );
        }

        cv::circle(
            canvas,
            scale_point(
                mean,
                scale
            ),
            7,
            cv::Scalar(255, 255, 255),
            2,
            cv::LINE_AA
        );
    }

    return canvas;
}

cv::Mat render_assignment_stage(
    const cv::Mat& image,
    const DetectionResult& result,
    const DetectionDiagnostics& diagnostics
) {
    cv::Mat canvas =
        make_stage_canvas(
            image,
            "Greedy 3x3 grid assignment"
        );

    if (canvas.empty()) {
        return {};
    }

    const double scale = std::clamp(
        900.0 /
            static_cast<double>(image.cols),
        1.0,
        12.0
    );

    for (const Patch& patch : result.patches) {
        cv::rectangle(
            canvas,
            scale_rectangle(
                patch.box,
                scale
            ),
            cv::Scalar(0, 220, 255),
            1,
            cv::LINE_AA
        );
    }

    const std::array<cv::Scalar, 3>
        row_colors = {
            cv::Scalar(80, 220, 80),
            cv::Scalar(255, 170, 40),
            cv::Scalar(220, 80, 220)
        };

    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            const int patch_id =
                diagnostics
                    .grid
                    .assigned_patch_ids[row][column];

            if (patch_id < 0) {
                continue;
            }

            const auto patch_iterator =
                std::find_if(
                    result.patches.begin(),
                    result.patches.end(),
                    [patch_id](
                        const Patch& patch
                    ) {
                        return patch.id ==
                            patch_id;
                    }
                );

            if (
                patch_iterator ==
                result.patches.end()
            ) {
                continue;
            }

            const cv::Rect rectangle =
                scale_rectangle(
                    patch_iterator->box,
                    scale
                );

            cv::rectangle(
                canvas,
                rectangle,
                row_colors[row],
                4,
                cv::LINE_AA
            );

            cv::putText(
                canvas,
                "r" +
                    std::to_string(row) +
                    "c" +
                    std::to_string(column) +
                    " #" +
                    std::to_string(patch_id),
                cv::Point(
                    rectangle.x,
                    rectangle.y +
                        std::min(
                            rectangle.height - 4,
                            22
                        )
                ),
                cv::FONT_HERSHEY_SIMPLEX,
                0.56,
                row_colors[row],
                2,
                cv::LINE_AA
            );
        }
    }

    cv::putText(
        canvas,
        "assigned=" +
            std::to_string(
                diagnostics
                    .grid
                    .assigned_count
            ) +
            "/9",
        cv::Point(18, 78),
        cv::FONT_HERSHEY_SIMPLEX,
        0.58,
        cv::Scalar(190, 190, 190),
        1,
        cv::LINE_AA
    );

    return canvas;
}

bool write_stage_image(
    const std::filesystem::path& path,
    const cv::Mat& image
) {
    if (image.empty()) {
        return false;
    }

    try {
        return cv::imwrite(
            path.string(),
            image
        );
    } catch (const cv::Exception&) {
        return false;
    }
}

}  // namespace

bool save_detection_stage_images(
    const std::filesystem::path& output_directory,
    const cv::Mat& image,
    const DetectionResult& result,
    const DetectionDiagnostics& diagnostics
) {
    if (image.empty()) {
        return false;
    }

    std::error_code directory_error;

    std::filesystem::create_directories(
        output_directory,
        directory_error
    );

    if (directory_error) {
        return false;
    }

    const cv::Mat raw_masks =
        render_mask_sheet(
            diagnostics.segmentation.raw_masks,
            diagnostics.segmentation.labels,
            "HSV threshold masks before morphology"
        );

    const cv::Mat cleaned_masks =
        render_mask_sheet(
            diagnostics.segmentation.cleaned_masks,
            diagnostics.segmentation.labels,
            "Color masks after morphology"
        );

    const cv::Mat candidates =
        render_candidates_stage(
            image,
            result
        );

    const cv::Mat geometry =
        render_geometry_stage(
            image,
            result,
            diagnostics
        );

    const cv::Mat assignment =
        render_assignment_stage(
            image,
            result,
            diagnostics
        );

    const cv::Mat final_result =
        render_detection_debug(
            image,
            result
        );

    bool ok = true;

    ok =
        write_stage_image(
            output_directory /
                "01_input.png",
            image
        ) &&
        ok;

    ok =
        write_stage_image(
            output_directory /
                "02_threshold_masks.png",
            raw_masks
        ) &&
        ok;

    ok =
        write_stage_image(
            output_directory /
                "03_cleaned_masks.png",
            cleaned_masks
        ) &&
        ok;

    ok =
        write_stage_image(
            output_directory /
                "04_candidates.png",
            candidates
        ) &&
        ok;

    ok =
        write_stage_image(
            output_directory /
                "05_geometry.png",
            geometry
        ) &&
        ok;

    ok =
        write_stage_image(
            output_directory /
                "06_grid_assignment.png",
            assignment
        ) &&
        ok;

    ok =
        write_stage_image(
            output_directory /
                "07_result.png",
            final_result
        ) &&
        ok;

    return ok;
}
