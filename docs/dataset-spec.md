# Dataset-Spec (gdl)

Diese Spezifikation ist der **Vertrag zwischen `gdl` und Downstream-Consumern**.

## 1. Ordner-Layout

```
<output-directory>/                 # --output-directory (Default: data/gii bzw. data/vvii)
├── raw-zips/                       # heruntergeladene Roharchive (.zip)
├── raw-extracted/                  # entpackte Inhalte, strukturiert je Gesetz
│   └── <KÜRZEL>/                   # stabile Pfad-Identifikatoren je Gesetz/Vorschrift
│       ├── xml.zip                 # (vor dem Entpacken)
│       └── *.xml                   # Gesetzestexte (Norm-XML)
├── manifest.json                   # Metadaten-Index (via `dataset-report`)
└── checksums.sha256               # Sha256-Hashes aller Dateien unter raw-zips/ + raw-extracted/
```

Für die Quelle `vvii` weichen die Roh-Bereiche ab (`raw-pages/`, `raw-artifacts/`)
und es gibt kein `raw-zips/`; die übrige Struktur (Manifest, Checksums, extrahierte
XML je Kürzel) entspricht demselben Vertrag.

## 2. Stabile Pfad-Identifikatoren

- **GII:** `<KÜRZEL>` ist das Pfadsegment der Download-URL vor `/xml.zip`,
  z. B. `bgb` aus `https://www.gesetze-im-internet.de/bgb/xml.zip`
  (`src/utils/download.cpp`, `try_get_file_and_folder`).
  Das entpackte Gesetz liegt unter `raw-extracted/<KÜRZEL>/`.
- Das Kürzel ist die **stabile ID für ein Gesetz**. Downstream adressiert
  Gesetze ausschließlich über diesen Ordnernamen.
- Umbenennungen oder Neufassungen mit anderem Kürzel sind bewusst **nicht**
  Teil dieses Vertrags (Law-Identity-Thematik → RIS).

## 3. XML-Format-Garantien (kanonisch & deterministisch)

Alle von `gdl` geschriebenen/formatierten XML-Dateien (`format-xml`,
`src/utils/xml.cpp`) erfüllen:

- **Einrückung:** 2 Leerzeichen (`pugi::format_indent`).
- **Deklaration:** `<?xml version="1.0"?>` (bzw. mit `encoding`, falls im
  Quelldokument vorhanden). Kein BOM.
- **Attribut-Reihenfolge:** wird aus der Quelle übernommen und **nicht**
  umsortiert → bei gleicher Eingabe deterministisch.
- **Keine zeitstempel- oder build-abhängigen Inhalte:** `gdl` schreibt keine
  Generierungszeit in die XML.
- **Plattformunabhängige Bytes:** Schreiben im Binärmodus (`std::ios::binary`)
  → identische Bytes unabhängig von LF/CRLF-Konvertierung.
- **Folge:** Mehrfaches Ausführen von `format-xml` auf denselben Daten erzeugt
  **byte-identische** Dateien (SHA256-stabil). Dies ist durch den Test
  `format_xml_files is deterministic ...` in `tests/utils/xml_utils_tests.cpp`
  abgesichert.

## 4. manifest.json

Wird durch den Befehl `dataset-report` erzeugt (`src/utils/dataset.cpp`).
Enthält den Metadaten-Index über die heruntergeladenen Gesetze. Die genaue
Feldstruktur ergibt sich aus der `dataset-report`-Implementierung; Consumer
sollten sich gegen die dort erzeugten Schlüssel validieren.

## 5. Versionshinweis

Änderungen an:

- Ordnernamen (`raw-zips/`, `raw-extracted/`, `<KÜRZEL>/`),
- Manifest-Struktur,
- XML-Serialisierung (Einrückung, Deklaration, Attribut-Reihenfolge),

gelten als Breaking und erfordern eine Anpassung und Versionserhöhung der Consumer.
