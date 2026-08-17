# User Guide

## Ontolaw GDL – CLI Benutzeranleitung

**gdl 1.0.0**
Copyright (c) Jens A. Koch, 2021-2026.

Ontolaw GDL ist ein Downloader für das auf <https://www.gesetze-im-internet.de> veröffentlichte Bundesrecht (Gesetze und Verordnungen).

### Aufruf

	gdl [OPTIONS] [ARGUMENTS]

### Optionen

| Kurz | Lang           | Beschreibung                                       |
|------|----------------|----------------------------------------------------|
| -h   | --help         | Zeigt diese Hilfe an.                              |
| -v   | --verbose      | Zeigt ausführlichere Laufzeit-Logs (inkl. Fortschritt). |
| -vv  | --verbose-very | Aktiviert sehr ausführliche cURL-Diagnoseausgabe und nutzt kompakte Fortschrittslogs. |
| -V   | --version      | Zeigt Versionsinformationen an.                    |
| -Vo  | --version-only | Zeigt nur die Versionsnummer an.                   |
| -Vj  | --version-json | Zeigt Versionsinformationen im JSON-Format an.     |
| -t   | --toc          | Lädt die Inhaltsverzeichnis-XML-Datei herunter.    |
| -d   | --download     | Lädt alle ZIP-Dateien herunter.                    |
| -u   | --unzip        | Entpackt alle ZIP-Dateien.                         |
| -f   | --format       | Formatiert alle XML-Dateien lesbar (Pretty Print). |
| -r   | --report       | Erstellt einen Datensatz-Report.                   |

### Argumente

| Argument           | Beschreibung                                 |
|--------------------|----------------------------------------------|
| -o=DIR, --out=DIR  | Zielordner setzen (Standard: data/gii) für TOC, ZIP, Entpacken, Format und Report. Führende `/` werden entfernt, Pfade sind damit relativ. |

### Datensatzlayout (Single Snapshot)

Ein Lauf erzeugt genau einen Datenstand im Zielordner.

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

- `raw-zips/`: Original-ZIPs aus dem Download.
- `raw-extracted/`: Entpackte XML-Dateien (Basis für `--format` und `--report`).
- `gii-toc.xml`: Inhaltsverzeichnis-Datei am Output-Root.
- `datensatz-bericht.md`: Report-Datei am Output-Root.

### Beispiele

**Hilfe anzeigen:**
	gdl --help

**Inhaltsverzeichnis herunterladen:**
	gdl --toc

**Alle ZIP-Dateien herunterladen und entpacken:**
	gdl --download --unzip

**Alle XML-Dateien formatieren:**
	gdl --format

**Datensatz-Report erstellen:**
	gdl --report

Der Report wird als `datensatz-bericht.md` im Zielordner (`--out`) gespeichert.

**Empfohlene Reihenfolge für einen vollständigen Datenstand:**
	gdl --toc --download --unzip --format --report

**Zielordner angeben:**
	gdl --download --out=/pfad/zum/ordner

Weitere Hinweise und fortgeschrittene Nutzung findest du im Developer Guide.

### Download-Fortschritt

- Interaktiv (TTY): Während `--download` zeigt GDL eine Fortschrittsanzeige mit Prozent/Balken sowie darunter die aktuell parallel laufenden Dateien.
- CI oder Non-TTY (z. B. Umleitung/Pipeline): GDL nutzt kompakte, append-only Checkpoint-Logs statt Zeilen-Updates.
