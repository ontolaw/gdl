# Ontolaw GDL

Ontolaw GDL ist ein CLI-Tool zum automatisierten Download deutscher Rechtstexte: Bundesgesetze und -verordnungen
von https://www.gesetze-im-internet.de sowie Verwaltungsvorschriften
von https://www.verwaltungsvorschriften-im-internet.de.

## Nutzung der Datensätze

Jeder Lauf erzeugt einen Snapshot des Datenbestands.
Jedes Artefakt stellt den vollständigen Datenstand zum Erstellungszeitpunkt bereit.
Es erfolgt keine inkrementelle Aktualisierung.
Die Datensätze sind für die maschinelle Weiterverarbeitung ausgelegt.

## Ablauf

- Der Downloader lädt das TOC XML herunter und liest die Links für die ZIP Dateien aus.
- Die ZIP Dateien werden unter `data/gii/raw-zips` gespeichert.
- Die ZIP Dateien werden nach `data/gii/raw-extracted` entpackt. Die Original-ZIPs bleiben erhalten.
- Alle XML Dateien in `data/gii/raw-extracted` werden formatiert (in eine Baumstruktur mit 2 Leerzeichen Einrückung).
- Es wird ein Datensatz-Report als `data/gii/datensatz-bericht.md` erzeugt.

TODO

- auto-commit + diff
- Das "data" Verzeichnis wird in die Branch "data" dieses git Repositories committet.
- Dieser Datenabruf erfolgt täglich (oder wöchentlich)?
- Die Daten werden mit den Daten des vorherigen Tages verglichen.
- Aus dem Diff wird eine tägliche und wöchentliche Änderungshistorie erzeugt.
- Die Änderungshistorie zeigt neu hinzugefügte Dateien, die Änderungen von bestehenden Dateien und entfernte Dateien an.

## Data Folder Layout

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

- Die GII-Verzeichnisstruktur bleibt unter `raw-zips/` und `raw-extracted/` erhalten (ein Ordner pro Gesetz).
- Viele Ordner enthalten nur eine XML-Datei.
- Einige Ordner enthalten zusätzliche Dateien (Einbettungsinhalte) in Form von Bildern (`jpg`) oder PDFs.
  Der Datensatz-Report listet Ordner mit XML-Dateien und weiteren Dateien gesondert auf.

## Dependencies

| Library       | License          |
| ------------- | ---------------- |
| spdlog        | MIT              |
| pugixml       | MIT              |
| fmt           | MIT              |
| curl          | Curl (MIT style) |
|  - zlib       | zlib             |
|  - minizip    | zlib             |
|  - openssl    | Apache-2.0       |
|  - zstd       | BSD license      |
|  - minizip    | zlib license     |

## Probleme

### Webseite: [gesetze-im-internet.de](https://www.gesetze-im-internet.de)

- Domain Routing Issues
  - Die Domain https://gesetze-im-internet.de ist nicht erreichbar. (Fehler: "ERR_NAME_NOT_RESOLVED")
  - Die Adresse https://www.gesetze-im-internet.de ist erreichbar, aber verwendet das unnötige "www"-Präfix.

- SSL Issue
  - Es gibt ein "ServerCertificateName mismatch", denn juris.de != gesetze-im-internet.de
  Das auf gesetzt-im-internet.de gegebene Cert gilt für juris.de.

### XML

- Problem "fundstelle typ=amtlich"
  - Im XML wird für Fundstellen immer der Typ "amtlich" angegeben, obwohl im gesamten System keine anderen Typen existieren. Die Typ-Angabe ist daher überflüssig. (Regex zum Nachweis: `(<fundstelle typ="(?!(amtlich)").*?>)`)

