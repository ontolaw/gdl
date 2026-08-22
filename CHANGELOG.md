# Changelog

All changes to the project will be documented in this file.

- The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
  The project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
- The date format is `YYYY-MM-DD`.
- The versions are linked to their Git tags via the reference section at the end of the document.
- The upcoming release `vNext` links to the changes between the latest version tag and git HEAD.

## [vNext] - unreleased

- "It was a bright day in April, and the clocks were striking thirteen." - 1984

## [0.2.0] - 2026-08-21

### Added
- Idempotenter GII-Download: bereits vorhandene ZIP-Dateien werden uebersprungen (Skip-existing), ein erneuter Lauf laedt nur neue Inhalte.
- `manifest.json` wird vom `--report`-Befehl erzeugt (Snapshot-Metadaten: `run_timestamp`, `source_urls`, `tool_version`, `file_counts`, `checksum_algorithm`).
- `checksums.sha256` wird fuer alle Artefakte unter `data/gii` und `data/vvii` erzeugt (SHA-256 im `sha256sum -c` kompatiblen Format).
- Vollstaendige `dataset-report`-Auswertung fuer die VVII-Quelle: es werden nun `raw-pages`, `raw-artifacts` sowie die Crawl-State-Dateien (`frontier-pages`, `visited-pages`, `discovered-artifacts`, `downloaded-artifacts`, `failed-artifacts`) berichtet.
- Dokumentation erweitert
- Versteckter Testmodus `--download-limit=N` (1-10) dokumentiert, um Downloads zum schnellen Testen der Pipeline (Unzip/Format/Report) zu begrenzen.

### Changed

- Der `--report`-Befehl erzeugt nun zusaetzlich `manifest.json` und `checksums.sha256` und listet deren Erzeugungsstatus im Report.

## [0.1.0] - 2021-01-01

### Changed
- Ausgabedatei des Reports von `dataset-report.md` zu `datensatz-bericht.md` umbenannt.
- Datensatz-Ausgabe auf ein Single-Snapshot-Layout mit `raw-zips/` und `raw-extracted/` unter `data/gii` geändert.
- `--download` speichert ZIP-Dateien nun in `data/gii/raw-zips`.
- `--unzip` entpackt nach `data/gii/raw-extracted` und behaelt die ZIP-Archive.

<!-- Section for Reference Links -->

[Unreleased]: https://github.com/ontolaw/gdl/compare/0.2.0...HEAD
[0.2.0]: https://github.com/ontolaw/gdl/compare/0.1.0...0.2.0
[0.1.0]: https://github.com/ontolaw/gdl/releases/tag/0.1.0
