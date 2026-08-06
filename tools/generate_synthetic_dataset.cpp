#include <opencv2/opencv.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int canvas_width = 420;
constexpr int canvas_height = 320;

constexpr float patch_size = 38.0F;
constexpr float horizontal_step = 60.0F;
constexpr float vertical_step = 78.0F;

using PatchPolygon = std::array<cv::Point2f, 4>;

enum class TransformKind {
    AFFINE,
    PERSPECTIVE
};

struct SceneConfig {
    std::string filename;
    std::string intended_label;
    std::string description;

    bool draw_marker = true;
    TransformKind transform_kind = TransformKind::AFFINE;

    float scale = 1.0F;
    float angle_degrees = 0.0F;
    cv::Point2f center{
        canvas_width / 2.0F,
        canvas_height / 2.0F
    };

    bool low_contrast = false;
    bool blur = false;
    bool noise = false;
    bool distractors = false;
    bool broken_spacing = false;

    // Applied to the bottom-middle cell when broken_spacing is enabled.
    float spacing_offset_x = 0.0F;
    float spacing_offset_y = 0.0F;

    // Zero-based marker cell index to omit, or -1 to draw all cells.
    int omitted_patch_index = -1;
};

struct GeneratedScene {
    SceneConfig config;
    cv::Mat image;
    double true_hull_coverage = 0.0;
};

const std::array<cv::Scalar, 9> marker_colors = {
    cv::Scalar(0, 0, 255),       // red
    cv::Scalar(0, 255, 0),       // green
    cv::Scalar(255, 0, 0),       // blue
    cv::Scalar(0, 255, 255),     // yellow
    // Hue is approximately 87: inside the cyan mask but outside
    // the current green and blue mask overlaps.
    cv::Scalar(230, 255, 0),     // cyan
    cv::Scalar(255, 0, 255),     // magenta
    cv::Scalar(255, 0, 0),       // blue
    cv::Scalar(0, 0, 255),       // red
    cv::Scalar(0, 255, 0)        // green
};

cv::Scalar scaled_color(
    const cv::Scalar& color,
    double factor
) {
    return cv::Scalar(
        color[0] * factor,
        color[1] * factor,
        color[2] * factor
    );
}

cv::Mat create_background() {
    cv::Mat image(
        canvas_height,
        canvas_width,
        CV_8UC3
    );

    for (int row = 0; row < image.rows; ++row) {
        const int shade =
            26 + (row * 20) / image.rows;

        image.row(row).setTo(
            cv::Scalar(shade, shade, shade)
        );
    }

    return image;
}

PatchPolygon make_local_patch(
    int row,
    int column
) {
    const float center_x =
        (static_cast<float>(column) - 1.0F) *
        horizontal_step;

    const float center_y =
        (static_cast<float>(row) - 1.0F) *
        vertical_step;

    const float half_size =
        patch_size / 2.0F;

    return {
        cv::Point2f(
            center_x - half_size,
            center_y - half_size
        ),
        cv::Point2f(
            center_x + half_size,
            center_y - half_size
        ),
        cv::Point2f(
            center_x + half_size,
            center_y + half_size
        ),
        cv::Point2f(
            center_x - half_size,
            center_y + half_size
        )
    };
}

cv::Matx33f affine_transform(
    float scale,
    float angle_degrees,
    const cv::Point2f& center
) {
    const float radians =
        angle_degrees *
        static_cast<float>(CV_PI) /
        180.0F;

    const float cosine =
        std::cos(radians) * scale;

    const float sine =
        std::sin(radians) * scale;

    return cv::Matx33f(
        cosine,
        -sine,
        center.x,

        sine,
        cosine,
        center.y,

        0.0F,
        0.0F,
        1.0F
    );
}

cv::Mat perspective_transform() {
    const std::vector<cv::Point2f> source = {
        {-90.0F, -105.0F},
        { 90.0F, -105.0F},
        { 90.0F,  105.0F},
        {-90.0F,  105.0F}
    };

    const std::vector<cv::Point2f> destination = {
        {105.0F,  65.0F},
        {325.0F,  92.0F},
        {292.0F, 268.0F},
        { 88.0F, 232.0F}
    };

    return cv::getPerspectiveTransform(
        source,
        destination
    );
}

PatchPolygon transform_polygon(
    const PatchPolygon& polygon,
    const cv::Mat& transform
) {
    std::vector<cv::Point2f> input(
        polygon.begin(),
        polygon.end()
    );

    std::vector<cv::Point2f> output;

    cv::perspectiveTransform(
        input,
        output,
        transform
    );

    if (output.size() != polygon.size()) {
        throw std::runtime_error(
            "Unexpected transformed polygon size"
        );
    }

    return {
        output[0],
        output[1],
        output[2],
        output[3]
    };
}

void draw_polygon(
    cv::Mat& image,
    const PatchPolygon& polygon,
    const cv::Scalar& color
) {
    std::vector<cv::Point> integer_points;
    integer_points.reserve(polygon.size());

    for (const auto& point : polygon) {
        integer_points.emplace_back(
            cvRound(point.x),
            cvRound(point.y)
        );
    }

    cv::fillConvexPoly(
        image,
        integer_points,
        color,
        cv::LINE_8
    );
}

void add_distractors(cv::Mat& image) {
    cv::rectangle(
        image,
        cv::Rect(18, 28, 18, 18),
        cv::Scalar(0, 0, 255),
        cv::FILLED
    );

    cv::rectangle(
        image,
        cv::Rect(365, 42, 20, 16),
        cv::Scalar(0, 255, 0),
        cv::FILLED
    );

    cv::circle(
        image,
        cv::Point(382, 270),
        12,
        cv::Scalar(255, 0, 255),
        cv::FILLED
    );

    cv::circle(
        image,
        cv::Point(44, 276),
        11,
        cv::Scalar(255, 255, 0),
        cv::FILLED
    );
}

void add_noise(
    cv::Mat& image,
    std::uint64_t seed
) {
    cv::RNG random(
        static_cast<std::uint64_t>(seed)
    );

    cv::Mat signed_image;
    image.convertTo(
        signed_image,
        CV_16SC3
    );

    cv::Mat noise(
        image.size(),
        CV_16SC3
    );

    random.fill(
        noise,
        cv::RNG::NORMAL,
        cv::Scalar::all(0),
        cv::Scalar::all(7)
    );

    signed_image += noise;

    signed_image.convertTo(
        image,
        CV_8UC3
    );
}

double hull_coverage(
    const std::vector<cv::Point2f>& corners
) {
    if (corners.size() < 3) {
        return 0.0;
    }

    std::vector<cv::Point2f> hull;
    cv::convexHull(corners, hull);

    const double hull_area =
        std::abs(cv::contourArea(hull));

    const double image_area =
        static_cast<double>(
            canvas_width * canvas_height
        );

    return hull_area / image_area;
}

GeneratedScene generate_scene(
    const SceneConfig& config,
    std::uint64_t seed
) {
    GeneratedScene generated;
    generated.config = config;
    generated.image = create_background();

    std::vector<cv::Point2f> marker_corners;

    if (config.draw_marker) {
        cv::Mat transform;

        if (
            config.transform_kind ==
            TransformKind::PERSPECTIVE
        ) {
            transform = perspective_transform();
        } else {
            transform = cv::Mat(
                affine_transform(
                    config.scale,
                    config.angle_degrees,
                    config.center
                )
            );
        }

        int patch_index = 0;

        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                PatchPolygon local_patch =
                    make_local_patch(
                        row,
                        column
                    );

                if (
                    patch_index ==
                    config.omitted_patch_index
                ) {
                    ++patch_index;
                    continue;
                }

                if (
                    config.broken_spacing &&
                    row == 2 &&
                    column == 1
                ) {
                    for (auto& point : local_patch) {
                        point.x +=
                            config.spacing_offset_x;

                        point.y +=
                            config.spacing_offset_y;
                    }
                }

                const PatchPolygon transformed =
                    transform_polygon(
                        local_patch,
                        transform
                    );

                cv::Scalar color =
                    marker_colors[patch_index];

                if (config.low_contrast) {
                    color =
                        scaled_color(
                            color,
                            0.72
                        );
                }

                draw_polygon(
                    generated.image,
                    transformed,
                    color
                );

                marker_corners.insert(
                    marker_corners.end(),
                    transformed.begin(),
                    transformed.end()
                );

                ++patch_index;
            }
        }
    }

    if (config.distractors) {
        add_distractors(
            generated.image
        );
    }

    if (config.blur) {
        cv::GaussianBlur(
            generated.image,
            generated.image,
            cv::Size(5, 5),
            1.1
        );
    }

    if (config.noise) {
        add_noise(
            generated.image,
            seed
        );
    }

    generated.true_hull_coverage =
        hull_coverage(marker_corners);

    return generated;
}

void save_contact_sheet(
    const std::filesystem::path& output_path,
    const std::vector<GeneratedScene>& scenes
) {
    constexpr int columns = 3;
    constexpr int tile_width = 280;
    constexpr int tile_height = 245;
    constexpr int preview_height = 205;

    const int rows =
        static_cast<int>(
            (scenes.size() + columns - 1) /
            columns
        );

    cv::Mat contact_sheet(
        rows * tile_height,
        columns * tile_width,
        CV_8UC3,
        cv::Scalar(20, 20, 20)
    );

    for (
        int index = 0;
        index < static_cast<int>(scenes.size());
        ++index
    ) {
        const int tile_row =
            index / columns;

        const int tile_column =
            index % columns;

        cv::Mat preview;

        cv::resize(
            scenes[index].image,
            preview,
            cv::Size(
                tile_width,
                preview_height
            ),
            0.0,
            0.0,
            cv::INTER_AREA
        );

        const cv::Rect destination(
            tile_column * tile_width,
            tile_row * tile_height,
            tile_width,
            preview_height
        );

        preview.copyTo(
            contact_sheet(destination)
        );

        cv::putText(
            contact_sheet,
            scenes[index].config.filename,
            cv::Point(
                tile_column * tile_width + 8,
                tile_row * tile_height +
                    preview_height + 24
            ),
            cv::FONT_HERSHEY_SIMPLEX,
            0.52,
            cv::Scalar(235, 235, 235),
            1,
            cv::LINE_AA
        );

        cv::putText(
            contact_sheet,
            scenes[index].config.intended_label,
            cv::Point(
                tile_column * tile_width + 8,
                tile_row * tile_height +
                    preview_height + 42
            ),
            cv::FONT_HERSHEY_SIMPLEX,
            0.44,
            cv::Scalar(150, 210, 255),
            1,
            cv::LINE_AA
        );
    }

    if (
        !cv::imwrite(
            output_path.string(),
            contact_sheet
        )
    ) {
        throw std::runtime_error(
            "Could not save contact sheet"
        );
    }
}

std::vector<SceneConfig> scene_configs() {
    return {
        {
            "clean_centered.png",
            "positive",
            "Regular centered marker"
        },
        {
            "small_marker.png",
            "positive",
            "Regular marker with low image coverage",
            true,
            TransformKind::AFFINE,
            0.56F,
            0.0F
        },
        {
            "rotated_20.png",
            "positive",
            "Marker rotated by 20 degrees",
            true,
            TransformKind::AFFINE,
            0.92F,
            20.0F
        },
        {
            "rotated_minus_28.png",
            "positive",
            "Marker rotated by -28 degrees",
            true,
            TransformKind::AFFINE,
            0.88F,
            -28.0F
        },
        {
            "perspective.png",
            "positive-challenge",
            "Projective distortion challenge",
            true,
            TransformKind::PERSPECTIVE
        },
        {
            "blurred_noisy.png",
            "positive-challenge",
            "Lower contrast marker with blur and noise",
            true,
            TransformKind::AFFINE,
            0.94F,
            8.0F,
            {
                canvas_width / 2.0F,
                canvas_height / 2.0F
            },
            true,
            true,
            true
        },
        {
            "distractors.png",
            "positive-challenge",
            "Valid marker with additional colored candidates",
            true,
            TransformKind::AFFINE,
            0.90F,
            -8.0F,
            {
                canvas_width / 2.0F,
                canvas_height / 2.0F
            },
            false,
            false,
            false,
            true
        },
        {
            "irregular_spacing.png",
            "warning",
            "One cell displaced beyond strict but within tolerant spacing",
            true,
            TransformKind::AFFINE,
            0.90F,
            0.0F,
            {
                canvas_width / 2.0F,
                canvas_height / 2.0F
            },
            false,
            false,
            false,
            false,
            true,
            88.0F,
            0.0F,
            -1
        },
        {
            "missing_patch.png",
            "negative",
            "Marker-like grid with its center cell removed",
            true,
            TransformKind::AFFINE,
            0.90F,
            0.0F,
            {
                canvas_width / 2.0F,
                canvas_height / 2.0F
            },
            false,
            false,
            false,
            false,
            false,
            0.0F,
            0.0F,
            4
        },
        {
            "blank.png",
            "negative",
            "Background without a marker",
            false
        }
    };
}

void write_manifest(
    const std::filesystem::path& path,
    const std::vector<GeneratedScene>& scenes
) {
    std::ofstream output(path);

    if (!output) {
        throw std::runtime_error(
            "Could not create manifest"
        );
    }

    output
        << "filename,"
        << "intended_label,"
        << "true_hull_coverage,"
        << "description\n";

    output
        << std::fixed
        << std::setprecision(6);

    for (const auto& scene : scenes) {
        output
            << scene.config.filename
            << ","
            << scene.config.intended_label
            << ","
            << scene.true_hull_coverage
            << ",\""
            << scene.config.description
            << "\"\n";
    }
}

void print_usage(const char* program_name) {
    std::cout
        << "Usage:\n"
        << "  "
        << program_name
        << " <output-directory>\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            print_usage(argv[0]);
            return 2;
        }

        const std::filesystem::path output_root(
            argv[1]
        );

        const std::filesystem::path image_directory =
            output_root / "images";

        std::filesystem::create_directories(
            image_directory
        );

        std::vector<GeneratedScene> generated_scenes;

        const auto configurations =
            scene_configs();

        generated_scenes.reserve(
            configurations.size()
        );

        std::uint64_t seed = 20260805;

        for (const auto& configuration : configurations) {
            GeneratedScene generated =
                generate_scene(
                    configuration,
                    seed++
                );

            const std::filesystem::path output_path =
                image_directory /
                configuration.filename;

            if (
                !cv::imwrite(
                    output_path.string(),
                    generated.image
                )
            ) {
                throw std::runtime_error(
                    "Could not save " +
                    output_path.string()
                );
            }

            std::cout
                << output_path
                << " true_coverage="
                << std::fixed
                << std::setprecision(2)
                << generated.true_hull_coverage * 100.0
                << "% intended="
                << configuration.intended_label
                << "\n";

            generated_scenes.push_back(
                std::move(generated)
            );
        }

        write_manifest(
            output_root / "manifest.csv",
            generated_scenes
        );

        save_contact_sheet(
            output_root / "contact_sheet.png",
            generated_scenes
        );

        std::cout
            << "\nGenerated "
            << generated_scenes.size()
            << " synthetic scenes in "
            << output_root
            << "\n";

        return 0;
    } catch (const std::exception& exception) {
        std::cerr
            << "Synthetic dataset generation failed: "
            << exception.what()
            << "\n";

        return 1;
    }
}
