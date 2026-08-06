# Dataset Provenance

## Private real-world evaluation set

The original implementation was developed and evaluated on 20 real-world
images containing substantial color clutter and false-positive candidates.
Those images are not redistributed in this repository.

The images were supplied for a technical assignment and are retained only as a
private local evaluation set. They are not included in the public build,
automated tests, documentation assets, or Git history.

Results obtained from that private set may be discussed as historical
engineering observations, but the set is not presented as a public or
independently reproducible benchmark.

## Public synthetic dataset

The public repository uses programmatically generated images for:

- reproducible automated tests;
- deterministic coverage ground truth;
- CLI examples;
- diagnostic visualizations;
- continuous integration.

The synthetic images test controlled properties such as:

- marker scale and image coverage;
- in-plane rotation;
- spacing irregularity;
- blur and pixel noise;
- colored distractors;
- negative images without a marker.

The synthetic dataset is intentionally not claimed to reproduce the full
difficulty or visual distribution of the private real-world images.

## Evaluation policy

The two datasets have different purposes:

| Dataset | Purpose | Distributed publicly |
|---|---|---|
| Private real-world set | Local qualitative evaluation and historical regression | No |
| Public synthetic set | Reproducibility, testing, examples and CI | Yes |

Improvements should be checked against both sets locally when possible, while
only the synthetic set is used by the public automated test suite.
