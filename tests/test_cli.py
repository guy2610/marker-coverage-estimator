#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Sequence


def fail(
    message: str,
    result: subprocess.CompletedProcess[str] | None = None,
) -> None:
    print(
        f"FAILED: {message}",
        file=sys.stderr,
    )

    if result is not None:
        print(
            f"\nReturn code: {result.returncode}",
            file=sys.stderr,
        )
        print(
            "\n--- stdout ---",
            file=sys.stderr,
        )
        print(
            result.stdout,
            file=sys.stderr,
        )
        print(
            "--- stderr ---",
            file=sys.stderr,
        )
        print(
            result.stderr,
            file=sys.stderr,
        )

    raise SystemExit(1)


def expect(
    condition: bool,
    message: str,
) -> None:
    if not condition:
        fail(message)


def run_process(
    command: Sequence[str],
    working_directory: Path,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        list(command),
        cwd=working_directory,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def expect_result(
    result: subprocess.CompletedProcess[str],
    expected_code: int,
    required_text: Sequence[str],
    description: str,
) -> None:
    if result.returncode != expected_code:
        fail(
            (
                f"{description}: expected exit code "
                f"{expected_code}"
            ),
            result,
        )

    combined_output = (
        result.stdout +
        result.stderr
    )

    for text in required_text:
        if text not in combined_output:
            fail(
                (
                    f"{description}: missing output "
                    f"{text!r}"
                ),
                result,
            )


def image_result_map(
    output: str,
) -> dict[str, str]:
    results: dict[str, str] = {}

    for line in output.splitlines():
        if (
            " marker_found " not in line and
            " marker_not_found " not in line
        ):
            continue

        image_path, separator, value = (
            line.partition(" ")
        )

        if not separator or not value:
            fail(
                f"Malformed image output line: {line!r}"
            )

        results[Path(image_path).name] = value

    return results


def coverage_percentage(
    output: str,
    filename: str,
) -> float:
    pattern = re.compile(
        (
            rf"{re.escape(filename)} "
            rf"marker_found coverage="
            rf"([0-9]+(?:\.[0-9]+)?)%"
        )
    )

    match = pattern.search(output)

    if match is None:
        fail(
            f"Could not extract coverage for {filename}"
        )

    return float(match.group(1))


def main() -> int:
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--executable",
        required=True,
        type=Path,
    )

    parser.add_argument(
        "--generator",
        required=True,
        type=Path,
    )

    parser.add_argument(
        "--project-root",
        required=True,
        type=Path,
    )

    arguments = parser.parse_args()

    executable = (
        arguments.executable.resolve()
    )

    generator = (
        arguments.generator.resolve()
    )

    project_root = (
        arguments.project_root.resolve()
    )

    expect(
        executable.is_file(),
        f"Executable does not exist: {executable}",
    )

    expect(
        generator.is_file(),
        f"Generator does not exist: {generator}",
    )

    help_result = run_process(
        [
            str(executable),
            "--help",
        ],
        project_root,
    )

    expect_result(
        help_result,
        0,
        [
            "--debug",
            "--visualize-dir",
        ],
        "Help command",
    )

    expect(
        "--legacy-output" not in help_result.stdout,
        (
            "Public CLI should not expose "
            "legacy private-data mode"
        ),
    )

    version_result = run_process(
        [
            str(executable),
            "--version",
        ],
        project_root,
    )

    expect_result(
        version_result,
        0,
        ["0.5.0"],
        "Version command",
    )

    unknown_option_result = run_process(
        [
            str(executable),
            "--unknown-option",
        ],
        project_root,
    )

    expect_result(
        unknown_option_result,
        2,
        ["Unknown option"],
        "Unknown option handling",
    )

    with tempfile.TemporaryDirectory(
        prefix="marker-coverage-synthetic-test-"
    ) as temporary_directory:
        dataset_root = Path(
            temporary_directory
        )

        generation_result = run_process(
            [
                str(generator),
                str(dataset_root),
            ],
            project_root,
        )

        expect_result(
            generation_result,
            0,
            ["Generated 10 synthetic scenes"],
            "Synthetic dataset generation",
        )

        image_directory = (
            dataset_root / "images"
        )

        expected_files = [
            "clean_centered.png",
            "small_marker.png",
            "rotated_20.png",
            "rotated_minus_28.png",
            "perspective.png",
            "blurred_noisy.png",
            "distractors.png",
            "irregular_spacing.png",
            "missing_patch.png",
            "blank.png",
        ]

        for filename in expected_files:
            image_path = (
                image_directory / filename
            )

            expect(
                image_path.is_file(),
                (
                    "Generated image is missing: "
                    f"{filename}"
                ),
            )

            expect(
                image_path.stat().st_size > 0,
                (
                    "Generated image is empty: "
                    f"{filename}"
                ),
            )

        manifest = (
            dataset_root / "manifest.csv"
        )

        expect(
            manifest.is_file(),
            "Synthetic manifest was not generated",
        )

        manifest_text = manifest.read_text(
            encoding="utf-8"
        )

        expect(
            (
                "small_marker.png,positive,"
                in manifest_text
            ),
            (
                "Manifest is missing "
                "small-marker ground truth"
            ),
        )

        expect(
            (
                "irregular_spacing.png,warning,"
                in manifest_text
            ),
            "Manifest is missing warning case",
        )

        expect(
            (
                "missing_patch.png,negative,"
                in manifest_text
            ),
            "Manifest is missing negative case",
        )

        clean_path = (
            image_directory /
            "clean_centered.png"
        )

        small_path = (
            image_directory /
            "small_marker.png"
        )

        warning_path = (
            image_directory /
            "irregular_spacing.png"
        )

        missing_path = (
            image_directory /
            "missing_patch.png"
        )

        blank_path = (
            image_directory /
            "blank.png"
        )

        clean_result = run_process(
            [
                str(executable),
                str(clean_path),
            ],
            project_root,
        )

        expect_result(
            clean_result,
            0,
            [
                "clean_centered.png marker_found",
                (
                    "Summary: detected=1 warnings=0 "
                    "not_detected=0 out of 1"
                ),
            ],
            "Clean synthetic marker",
        )

        clean_coverage = coverage_percentage(
            clean_result.stdout,
            "clean_centered.png",
        )

        expect(
            20.0 <= clean_coverage <= 30.0,
            (
                "Clean marker coverage is outside "
                "the expected range"
            ),
        )

        small_result = run_process(
            [
                str(executable),
                str(small_path),
            ],
            project_root,
        )

        expect_result(
            small_result,
            0,
            [
                "small_marker.png marker_found",
                (
                    "Summary: detected=1 warnings=0 "
                    "not_detected=0 out of 1"
                ),
            ],
            "Low-coverage synthetic marker",
        )

        small_coverage = coverage_percentage(
            small_result.stdout,
            "small_marker.png",
        )

        expect(
            5.0 <= small_coverage <= 10.0,
            (
                "Small marker coverage is outside "
                "the expected range"
            ),
        )

        warning_result = run_process(
            [
                str(executable),
                str(warning_path),
            ],
            project_root,
        )

        expect_result(
            warning_result,
            0,
            [
                "irregular_spacing.png marker_found",
                "warning=IRREGULAR_SPACING",
                (
                    "Summary: detected=0 warnings=1 "
                    "not_detected=0 out of 1"
                ),
            ],
            "Synthetic warning case",
        )

        missing_result = run_process(
            [
                str(executable),
                str(missing_path),
            ],
            project_root,
        )

        expect_result(
            missing_result,
            0,
            [
                (
                    "missing_patch.png "
                    "marker_not_found "
                    "reason=GRID_ASSIGNMENT_FAILED"
                ),
                (
                    "Summary: detected=0 warnings=0 "
                    "not_detected=1 out of 1"
                ),
            ],
            "Missing-patch negative",
        )

        blank_result = run_process(
            [
                str(executable),
                str(blank_path),
            ],
            project_root,
        )

        expect_result(
            blank_result,
            0,
            [
                (
                    "blank.png marker_not_found "
                    "reason=FEW_PATCHES"
                ),
                (
                    "Summary: detected=0 warnings=0 "
                    "not_detected=1 out of 1"
                ),
            ],
            "Blank-image negative",
        )

        stable_filenames = [
            "clean_centered.png",
            "small_marker.png",
            "rotated_20.png",
            "rotated_minus_28.png",
            "perspective.png",
            "irregular_spacing.png",
            "missing_patch.png",
            "blank.png",
        ]

        forward_images = [
            str(image_directory / filename)
            for filename in stable_filenames
        ]

        reverse_images = list(
            reversed(forward_images)
        )

        forward_result = run_process(
            [
                str(executable),
                *forward_images,
            ],
            project_root,
        )

        expect_result(
            forward_result,
            0,
            [
                (
                    "Summary: detected=5 warnings=1 "
                    "not_detected=2 out of 8"
                ),
            ],
            "Forward synthetic regression run",
        )

        repeated_result = run_process(
            [
                str(executable),
                *forward_images,
            ],
            project_root,
        )

        expect_result(
            repeated_result,
            0,
            [
                (
                    "Summary: detected=5 warnings=1 "
                    "not_detected=2 out of 8"
                ),
            ],
            "Repeated synthetic regression run",
        )

        expect(
            (
                forward_result.stdout ==
                repeated_result.stdout
            ),
            (
                "Repeated synthetic runs must "
                "produce identical stdout"
            ),
        )

        reverse_result = run_process(
            [
                str(executable),
                *reverse_images,
            ],
            project_root,
        )

        expect_result(
            reverse_result,
            0,
            [
                (
                    "Summary: detected=5 warnings=1 "
                    "not_detected=2 out of 8"
                ),
            ],
            "Reverse synthetic regression run",
        )

        expect(
            (
                image_result_map(
                    forward_result.stdout
                ) ==
                image_result_map(
                    reverse_result.stdout
                )
            ),
            (
                "Per-image results must be "
                "independent of batch order"
            ),
        )

        visualization_directory = (
            dataset_root / "annotated"
        )

        visualization_result = run_process(
            [
                str(executable),
                "--visualize-dir",
                str(visualization_directory),
                str(clean_path),
            ],
            project_root,
        )

        expect_result(
            visualization_result,
            0,
            ["clean_centered.png marker_found"],
            "Visualization generation",
        )

        visualization_file = (
            visualization_directory /
            "clean_centered_annotated.png"
        )

        expect(
            visualization_file.is_file(),
            (
                "Annotated visualization "
                "was not created"
            ),
        )

        expect(
            visualization_file.stat().st_size > 0,
            "Annotated visualization is empty",
        )

        diagnostics_directory = (
            dataset_root / "diagnostics"
        )

        diagnostics_result = run_process(
            [
                str(executable),
                "--diagnostics-dir",
                str(diagnostics_directory),
                str(clean_path),
            ],
            project_root,
        )

        expect_result(
            diagnostics_result,
            0,
            ["clean_centered.png marker_found"],
            "Diagnostic stage generation",
        )

        diagnostic_case_directory = (
            diagnostics_directory /
            "clean_centered"
        )

        expected_stage_files = [
            "01_input.png",
            "02_threshold_masks.png",
            "03_cleaned_masks.png",
            "04_candidates.png",
            "05_geometry.png",
            "06_grid_assignment.png",
            "07_result.png",
        ]

        for stage_filename in expected_stage_files:
            stage_path = (
                diagnostic_case_directory /
                stage_filename
            )

            expect(
                stage_path.is_file(),
                (
                    "Diagnostic stage image "
                    f"was not created: {stage_filename}"
                ),
            )

            expect(
                stage_path.stat().st_size > 0,
                (
                    "Diagnostic stage image "
                    f"is empty: {stage_filename}"
                ),
            )

    print(
        "All synthetic CLI regression tests passed."
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
