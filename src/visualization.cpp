#include "visualization.hpp"

#include "types.hpp"

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <string>

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
