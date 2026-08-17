# Changelog

All changes to the project will be documented in this file.

- The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
  The project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
- The date format is `YYYY-MM-DD`.
- The versions are linked to their Git tags via the reference section at the end of the document.
- The upcoming release `vNext` links to the changes between the latest version tag and git HEAD.

## [vNext] - unreleased

- "It was a bright day in April, and the clocks were striking thirteen." - 1984

## [0.1.0] - 2021-01-01

- Renamed report output file from `dataset-report.md` to `datensatz-bericht.md`.
- Changed dataset output to a single-snapshot layout with `raw-zips/` and `raw-extracted/` under `data/gii`.
- `--download` now stores ZIP files in `data/gii/raw-zips`; `--unzip` extracts to `data/gii/raw-extracted` and keeps ZIP archives.
- Migration hint: scripts expecting extracted XML directly under `data/gii/<gesetz>/` must switch to `data/gii/raw-extracted/<gesetz>/`.

<!-- Section for Reference Links -->

[Unreleased]: https://github.com/ontolaw/gdl/compare/0.1.0...HEAD
[0.1.0]: https://github.com/ontolaw/gdl/releases/tag/0.1.0
