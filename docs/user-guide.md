# User Guide

## Ontolaw GDL – CLI Benutzeranleitung

**gdl 1.0.0**
Copyright (c) Jens A. Koch, 2021-2026.

Ontolaw GDL ist ein Downloader für das auf <https://www.gesetze-im-internet.de> veröffentlichte Bundesrecht (Gesetze und Verordnungen).

### Aufruf

	gdl [gii|vvii|bgbl] [OPTIONEN] [BEFEHLE]

### Optionen

Allgemeine Optionen:
Optionen, die das Verhalten des Tools steuern.

  -h   --help                 Zeige diese Hilfe an.
  -V   --version              Zeige Versionsinformationen an.
  -Vo  --version-only         Zeige nur die Versionsnummer an.
  -Vj  --version-json         Zeige Versionsinformationen im JSON-Format an.
  -Vb  --version-build        Zeige Build-Informationen an.
  -Vbj --version-build-json   Zeige Build-Informationen im JSON-Format an.

Funktionale Optionen:
Optionen, die spezifische Aufgaben ausführen und kombiniert werden können.

  -t   --toc                  Das Inhaltsverzeichnis (XML) herunterladen.
  -d   --download             Alle Dateien gemäß Quelle herunterladen (gii: ZIP/XML, vvii: HTML + Artefakte).
  -u   --unzip                Archivdateien entpacken.
  -f   --format               XML-Dateien formatiert ausgeben.
  -r   --report               Datensatz-Bericht nach Validierung erstellen.

### Argumente

| Argument             | Beschreibung                                 |
|----------------------|----------------------------------------------|
| -v   --verbose       | Ausführliche Protokollierung (Info/Fortschritt/Warnungen).
|  -vv  --verbose-very | Sehr ausführliche Protokollierung (inkl. cURL-Details).
| gii \| vvii          | Quelle als Subcommand-Präfix wählen: `gdl gii …` bzw. `gdl vvii …` (Standard = gii).
|       --source=SRC   | Alternative zur Präfix-Wahl: Downloadquelle explizit setzen (`--source=gii` oder `--source=vvii`).
| bgbl                 | Befehlsgruppe für Bundesgesetzblätter (bgbl.de XAVER) wählen: `gdl bgbl …`.
|       --year=JAHR    | BGBl-Jahrgang (z. B. `--year=1949`). Erforderlich für `download-issue` und `download-year`.
|       --nr=NR        | BGBl-Ausgabenummer innerhalb des Jahrgangs (z. B. `--nr=1`). Erforderlich für `download-issue`.
|       --part=TEIL    | BGBl-Teil: `1`/`I` für Teil I, `2`/`II` für Teil II (Standard = 1).
|       --from=JAHR    | Erstes Jahr eines Jahrgänge-Bereichs (für `download-range`).
|       --to=JAHR      | Letztes Jahr eines Jahrgänge-Bereichs (für `download-range`).
|       --jobs=ANZAHL  | Anzahl paralleler Downloads für `download-year` und `download-range` (Standard = 4, minimum 1).
|       --bgbl-source=SRC | BGBl-Quelle wählen: `xaver` (bgbl.de, Standard), `og` (OffeneGesetze.de) oder `ris` (Bund-Rechtsinformationssystem / recht.bund.de).
| -o=DIR, --out=DIR, -out=DIR | Zielordner setzen (Standard: gii=data/gii, vvii=data/vvii) für TOC, ZIP, Entpacken, Format und Report. Führende `/` werden entfernt, Pfade sind damit relativ. |

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

#### Layout für `--source=vvii`

Bei der VVII-Quelle entsteht eine andere Ordnerstruktur, da hier gecrawlt wird
(HTML-Seiten plus Artefakte wie PDF/DOC/XML):

```text
data/vvii/
	vvii-toc.xml
	raw-pages/
		<host>/<seite>.html
	raw-artifacts/
		<host>/<artefakt>.<ext>
	state/
		frontier-pages.txt
		visited-pages.txt
		discovered-artifacts.txt
		downloaded-artifacts.txt
		failed-artifacts.txt
```

- `raw-pages/`: Heruntergeladene HTML-Seiten der VVII-Webseite.
- `raw-artifacts/`: Gefundene Artefakte (PDF, DOC, XML usw.).
- `state/`: Crawl-Status (Frontier, besuchte Seiten, entdeckte/heruntergeladene/fehlgeschlagene Artefakte).

### Bundesgesetzblatt (bgbl.de / OffeneGesetze.de / Bund RIS)

Über die Befehlsgruppe `bgbl` können einzelne Bundesgesetzblatt-Ausgaben
(Teil I und II) als PDF heruntergeladen werden. Dafür stehen drei Quellen
zur Wahl:

- **bgbl.de XAVER** (Standard, `--bgbl-source=xaver`): lädt die „Komplette
  Ausgabe" der gewählten Ausgabe von der XAVER-Plattform des bgbl.de.
- **OffeneGesetze.de** (`--bgbl-source=og` bzw. `--bgbl-source=offenegesetze`):
  nutzt die offene REST-API von OffeneGesetze.de
  (`api.offenegesetze.de`). Dort liegt pro Ausgabenummer bereits eine
  konsolidierte PDF bereit, sodass kein mehrstufiges XAVER-Navigations-
  Handling nötig ist. Die Daten von OffeneGesetze.de reichen (Stand der
  API) bis einschließlich 2022.
- **Bund-Rechtsinformationssystem / RIS** (`--bgbl-source=ris` bzw.
  `--bgbl-source=bund`): die offizielle Verkündungsplattform
  recht.bund.de bzw. deren REST-API (`rechtsinformationen.bund.de`). Die
  PDF-URL ist deterministisch aus dem ELI-Schema ableitbar
  (`…/eli/bund/BGBl-<TEIL>/<JAHR>/<NR>/regelungstext.pdf?__blob=publicationFile`),
  weshalb einzelne Ausgaben ohne Navigationsaufruf geladen werden. Die
  RIS-Quelle deckt die neueren (2023+) Jahrgang ab; Ausgabenummern können
  ein Zusatz-Suffix tragen (z. B. `67-2`).

```text
gdl bgbl download-toc   --year=1949 --part=1
gdl bgbl download-issue  --year=1949 --nr=1 --part=1
gdl bgbl download-year   --year=1949 --part=1
gdl bgbl download-range  --from=1949 --to=1950 --part=1
```

Die Quelle wird pro Befehl über `--bgbl-source=` gewählt, z. B.:

```text
gdl bgbl download-year --year=2018 --part=1 --bgbl-source=og
gdl bgbl download-year --year=2026 --part=1 --bgbl-source=ris
```

- `download-toc`: Listet die verfügbaren Ausgaben eines Jahrgangs (Nummer +
  Datum) — so ist erkennbar, welche `--nr=`-Werte gültig sind
  (`--year`, optional `--part`).
- `download-issue`: Eine einzelne Ausgabe (`--year` + `--nr`, optional `--part`).
- `download-year`: Alle Ausgaben eines Jahrgangs (`--year`, optional `--part`).
- `download-range`: Alle Ausgaben eines Jahrgänge-Bereichs (`--from` + `--to`, optional `--part`).

`download-year` und `download-range` laden die einzelnen Ausgaben
standardmäßig mit 4 parallelen Worker-Threads herunter (über `--jobs`
anpassbar). Jeder Worker nutzt eine eigene Session, da die XAVER-`media.xav`
Download-URL einen sitzungsgebundenen CSRF-Token enthält.

Die PDFs werden als `bgbl<TEIL>_<JAHR>_Nr<NR>.pdf` in den Zielordner
(`--out`) geschrieben.

### Beispiele

**Hilfe anzeigen:**
	gdl --help

**Inhaltsverzeichnis herunterladen:**
	gdl --toc

**Inhaltsverzeichnis für GII explizit (Präfix-Form):**
	gdl gii --toc

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

**VVII-Quelle (Verwaltungsvorschriften) mit eigenem Ordner:**
	gdl vvii --toc --download --unzip --format --report

**VVII-Daten in einen eigenen Zielordner laden:**
	gdl vvii --out=data/vvii-test --toc --download --unzip --format --report

**Bundesgesetzblatt Teil I, Ausgabe Nr. 1/1949 als PDF laden:**
	gdl bgbl download-issue --year=1949 --nr=1 --part=1 --out=data/bgbl

**Verfügbare Ausgaben des BGBl I 1949 auflisten (zeigt gültige `--nr=`):**
	gdl bgbl download-toc --year=1949 --part=1

**Alle Ausgaben des BGBl I von 1949 bis 1950 laden:**
	gdl bgbl download-range --from=1949 --to=1950 --part=1 --out=data/bgbl

Hinweis: Die Quelle lässt sich wahlweise auch weiterhin über den Global-Parameter
`--source=` wählen; das Präfix `gii`/`vvii` ist die bevorzugte, kürzere Form.

Weitere Hinweise und fortgeschrittene Nutzung findest du im Developer Guide.

### Download-Fortschritt

- Interaktiv (TTY): Während `--download` zeigt GDL eine Fortschrittsanzeige mit Prozent/Balken sowie darunter die aktuell parallel laufenden Dateien.
- CI oder Non-TTY (z. B. Umleitung/Pipeline): GDL nutzt kompakte, append-only Checkpoint-Logs statt Zeilen-Updates.
