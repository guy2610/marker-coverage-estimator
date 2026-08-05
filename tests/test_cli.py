#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Sequence


def fail(message: str, result: subprocess.CompletedProcess[str] | None = None) -> None:
    print(f"FAILED: {message}", file=sys.stderr)

    if result is not None:
        print(
            f"\nReturn code: {result.returncode}",
            file=sys.stderr,
        )
        print("\n--- stdout ---", file=sys.stderr)
        print(result.stdout, file=sys.stderr)
        print("--- stderr ---", file=sys.stderr)
        print(result.stderr, file=sys.stderr)

    raise SystemExit(1)


def expect(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


def run_command(
    executable: Path,
    project_root: Path,
    arguments: Sequence[str],
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(executable), *arguments],
        cwd=project_root,
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
            f"{description}: expected exit code {expected_code}",
            result,
        )

    combined_output = result.stdout + result.stderr

    for text in required_text:
        if text not in combined_output:
            fail(
                f"{description}: missing output {text!r}",
                result,
            )


def image_result_map(output: str) -> dict[str, str]:
    results: dict[str, str] = {}

    for line in output.splitlines():
        if not line.startswith("data/hi"):
            continue

        image_path, _, value = line.partition(" ")

        if not value:
            fail(f"Malformed image output line: {line!r}")

        results[image_path] = value

    return results


def main() -> int:
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--executable",
        required=True,
        type=Path,
    )

    parser.add_argument(
        "--project-root",
        required=True,
        type=Path,
    )

    arguments = parser.parse_args()

    executable = arguments.executable.resolve()
    project_root = arguments.project_root.resolve()

    expect(
        executable.is_file(),
        f"Executable does not exist: {executable}",
    )

    help_result = run_command(
        executable,
        project_root,
        ["--help"],
    )

    expect_result(
        help_result,
        0,
        [
            "--debug",
            "--legacy-output",
            "--visualize-dir",
        ],
        "Help command",
    )

    version_result = run_command(
        executable,
        project_root,
        ["--version"],
    )

    expect_result(
        version_result,
        0,
        ["0.4.0"],
        "Version command",
    )

    unknown_option_result = run_command(
        executable,
        project_root,
        ["--unknown-option"],
    )

    expect_result(
        unknown_option_result,
        2,
        ["Unknown option"],
        "Unknown option handling",
    )

    hi8_result = run_command(
        executable,
        project_root,
        ["data/hi8.png"],
    )

    expect_result(
        hi8_result,
        0,
        [
            "data/hi8.png marker_found coverage=38.6%",
            "Summary: detected=1 warnings=0 not_detected=0 out of 1",
        ],
        "Low-coverage valid marker",
    )

    hi6_result = run_command(
        executable,
        project_root,
        ["data/hi6.png"],
    )

    expect_result(
        hi6_result,
        0,
        [
            "data/hi6.png marker_not_found "
            "reason=SPACING_VALIDATION_FAILED",
            "Summary: detected=0 warnings=0 not_detected=1 out of 1",
        ],
        "Spacing failure",
    )

    hi11_result = run_command(
        executable,
        project_root,
        ["data/hi11.png"],
    )

    expect_result(
        hi11_result,
        0,
        [
            "data/hi11.png marker_found coverage=57.0% "
            "warning=IRREGULAR_SPACING",
            "Summary: detected=0 warnings=1 not_detected=0 out of 1",
        ],
        "Warning detection",
    )

    forward_images = [
        f"data/hi{index}.png"
        for index in range(1, 21)
    ]

    reverse_images = list(reversed(forward_images))

    forward_result = run_command(
        executable,
        project_root,
        forward_images,
    )

    expect_result(
        forward_result,
        0,
        [
            "Summary: detected=15 warnings=2 "
            "not_detected=3 out of 20",
        ],
        "Forward dataset run",
    )

    repeated_result = run_command(
        executable,
        project_root,
        forward_images,
    )

    expect_result(
        repeated_result,
        0,
        [
            "Summary: detected=15 warnings=2 "
            "not_detected=3 out of 20",
        ],
        "Repeated dataset run",
    )

    expect(
        forward_result.stdout == repeated_result.stdout,
        "Repeated dataset runs must produce identical stdout",
    )

    reverse_result = run_command(
        executable,
        project_root,
        reverse_images,
    )

    expect_result(
        reverse_result,
        0,
        [
            "Summary: detected=15 warnings=2 "
            "not_detected=3 out of 20",
        ],
        "Reverse dataset run",
    )

    expect(
        image_result_map(forward_result.stdout)
        == image_result_map(reverse_result.stdout),
        "Per-image results must be independent of batch order",
    )

    legacy_result = run_command(
        executable,
        project_root,
        ["--legacy-output", *forward_images],
    )

    expect_result(
        legacy_result,
        1,
        [
            "data/hi6.png 0%",
            "data/hi8.png 0%",
            "Summary: passed=18 failed=2 out of 20",
        ],
        "Legacy regression",
    )

    with tempfile.TemporaryDirectory(
        prefix="marker-coverage-test-"
    ) as temporary_directory:
        output_directory = Path(temporary_directory)

        visualization_result = run_command(
            executable,
            project_root,
            [
                "--visualize-dir",
                str(output_directory),
                "data/hi8.png",
            ],
        )

        expect_result(
            visualization_result,
            0,
            ["data/hi8.png marker_found coverage=38.6%"],
            "Visualization generation",
        )

        visualization_file = (
            output_directory /
            "hi8_annotated.png"
        )

        expect(
            visualization_file.is_file(),
            "Annotated visualization file was not created",
        )

        expect(
            visualization_file.stat().st_size > 0,
            "Annotated visualization file is empty",
        )

    print("All CLI regression tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
