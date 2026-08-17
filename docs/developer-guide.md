# Developer Guide

## Architekturübersicht

Das Projekt ist modular aufgebaut und folgt einer klaren Trennung von Verantwortlichkeiten:

- **src/app/**: Einstiegspunkt (`main.cpp`), zentrale Konfiguration
- **src/commands/**: Einzelne Kommandos als separate Module (z.B. Download, Unzip, Report)
- **src/utils/**: Hilfsfunktionen für Farben, Download, Dateisystem, XML, Unzip
- **cmake/**: Build-Konfigurationen und Presets
- **vcpkg_installed/**: Externe Abhängigkeiten (z.B. pugixml, curl, spdlog)

## Datensatzlayout (Single Snapshot)

Ein Artefakt enthält genau einen Datenstand unter einem gemeinsamen Output-Root (Standard: `data/gii`), ohne `current`/`latest`-Symlink-Indirektion.

```text
data/gii/
	gii-toc.xml
	datensatz-bericht.md
	manifest.json
	checksums.sha256
	raw-zips/
		<gesetz>/xml.zip
	raw-extracted/
		<gesetz>/*.xml
```

- `--download` speichert ZIPs unter `raw-zips/`.
- `--unzip` entpackt nach `raw-extracted/` und belässt ZIPs in `raw-zips/`.
- `--format` verarbeitet XML-Dateien unter `raw-extracted/`.
- `--report` analysiert `raw-extracted/` und schreibt `datensatz-bericht.md` am Output-Root.

### Minimalformat für Manifest und Checksums

`manifest.json` sollte mindestens enthalten:

- `run_timestamp` (UTC, ISO-8601)
- `source_urls` (inkl. TOC URL)
- `tool_version`
- `file_counts` (`raw_zips`, `raw_extracted_xml`)
- `checksum_algorithm` (`sha256`)

`checksums.sha256` enthält SHA-256-Prüfsummen für Artefakte unter `data/gii`.

## Build-Prozess

Das Projekt ist in C++ geschrieben.
Es verwendet CMake als Build-Werkzeug und vcpkg für das Abhängigkeitsmanagement.

### Voraussetzungen
- CMake (>=3.25 empfohlen)
- vcpkg (wird automatisch genutzt, siehe CMakePresets)
- C++20-kompatibler Compiler

### Build-Schritte (Linux)
```sh
cmake --preset gcc14-x64-linux-dbg
cmake --build --preset gcc14-x64-linux-dbg
```

Das erzeugte Binary befindet sich im Build-Output-Ordner, hier `out`.

### Testing
```sh
cmake --build --preset gcc14-x64-linux-dbg --target gdl_tests
cd out/build/gcc14-x64-linux-dbg
ctest --output-on-failure
```

### Coverage

Code-Coverage wird mit Clang's source-based Coverage (llvm-profdata/llvm-cov) generiert.
Die CI-Pipeline nutzt den `clang22-x64-linux-dbg-cov`-Preset.

**Schritt 1 – Konfigurieren:**

```sh
cmake --preset clang22-x64-linux-dbg-cov -B out/build/clang22-x64-linux-dbg-cov
```

**Schritt 2 – Bauen und Tests ausführen:**

```sh
cmake --build out/build/clang22-x64-linux-dbg-cov
LLVM_PROFILE_FILE=out/build/clang22-x64-linux-dbg-cov/coverage/%p.profraw \
  ctest --test-dir out/build/clang22-x64-linux-dbg-cov --output-on-failure
```

**Schritt 3 – Coverage-Daten zusammenführen und Report erzeugen:**

```sh
BUILD=out/build/clang22-x64-linux-dbg-cov
llvm-profdata merge -sparse "$BUILD/coverage/"*.profraw -o "$BUILD/coverage/coverage.profdata"

BINARIES=($(find "$BUILD" -maxdepth 3 -type f -executable \( -name "*_test" -o -name "*_tests" \)))
llvm-cov report \
  -instr-profile="$BUILD/coverage/coverage.profdata" \
  "${BINARIES[@]}" \
  --ignore-filename-regex='(vcpkg_installed|build|tests)'

llvm-cov export -format=lcov \
  -instr-profile="$BUILD/coverage/coverage.profdata" \
  "${BINARIES[@]}" \
  --ignore-filename-regex='(vcpkg_installed|build|tests)' \
  > "$BUILD/coverage/coverage.lcov"
```

Die Ergebnisse liegen unter `out/build/clang22-x64-linux-dbg-cov/coverage/`:

- `coverage.lcov` – LCOV-Report (z.B. für Codecov/Coveralls)
- `coverage.profdata` – merged Profile-Daten

### Abhängigkeiten

Alle externen Libraries werden über vcpkg eingebunden (z.B. pugixml, curl, spdlog, minizip, zlib, fmt).

## Erweiterung

### Neue Kommandos hinzufügen

1. Neues Command-Modul in `src/commands/` anlegen (z.B. `cmd.neu.cpp` und `cmd.neu.h`).
2. In der zentralen Command-Registrierung (`main.cpp` oder entsprechender Dispatcher) einbinden.
3. Option und Argumente dokumentieren.

### Utilities erweitern

Neue Hilfsfunktionen in `src/utils/` anlegen und in den jeweiligen Commands nutzen.

## Tipps für Entwickler

- Bestehende Commands als Vorlage für neue Funktionen nutzen.
- Fehlerausgaben und Logging über spdlog realisieren.
- Für XML-Verarbeitung pugixml verwenden.
- Für Downloads und HTTP curl nutzen.
- Code-Style und Struktur beibehalten (Header/Source-Trennung, sprechende Namen).

## Weiterführende Hinweise

- Siehe `CMakeLists.txt` und `CMakePresets.json` für Build-Optionen.
- Siehe User Guide für CLI-Optionen und Beispiele.

Fragen oder Beiträge? Bitte im Repository melden.
