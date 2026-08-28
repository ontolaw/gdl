# Ontolaw GDL

Ontolaw GDL ist ein CLI-Tool zum automatisierten Download deutscher Rechtstexte.
Unterstützt werden die folgenden Quellen:
- Download des BGBL-1 von https://www.recht.bund.de/ (amtlichen Fassung des Bundesgesetzblatts, die seit 1. Januar 2023 bereitgestellt wird)
- Download des BGBL-1 von 1949 bis heute von https://www.bgbl.de oder https://www.offenegesetze.de
- Download von Bundesgesetzen und -verordnungen von https://www.gesetze-im-internet.de (nichtamtliche Fassung).
- Download von Verwaltungsvorschriften von https://www.verwaltungsvorschriften-im-internet.de.

## Nutzung der Datensätze

- Jeder Lauf erzeugt einen Snapshot des Datenbestands.
- Jedes Artefakt stellt den vollständigen Datenstand zum Erstellungszeitpunkt bereit.
- Es erfolgt keine inkrementelle Aktualisierung.
- Die Datensätze sind für die maschinelle Weiterverarbeitung ausgelegt.

## Ablauf für Download und Verarbeitung am Beispiel der GII-Daten

- Der Downloader lädt das TOC XML herunter und liest die Links für die ZIP Dateien aus.
- Die ZIP Dateien werden unter `data/gii/raw-zips` gespeichert.
- Die ZIP Dateien werden nach `data/gii/raw-extracted` entpackt. Die Original-ZIPs bleiben erhalten.
- Alle XML Dateien in `data/gii/raw-extracted` werden formatiert (in eine Baumstruktur mit 2 Leerzeichen Einrückung).
- Es wird ein Datensatz-Report als `data/gii/datensatz-bericht.md` erzeugt.

## Dokumentation

- [User Guide](docs/user-guide.md) – CLI-Optionen und Arbeitsweise.
- [Developer Guide](docs/developer-guide.md) – Architektur, Dataset-Layout, Build, Tests, Abhängigkeiten.

## Probleme

Die folgenden Probleme wurden bei der Entwicklung festgestellt.
Sie betreffen die Webseite der Datenquellen, sind also Upstream Issues,
und können nur von den Betreibern der Webseite behoben werden.

### Webseite: [gesetze-im-internet.de](https://www.gesetze-im-internet.de)

- Domain Routing Issues
  - Die Domain https://gesetze-im-internet.de ist nicht erreichbar. (Fehler: "ERR_NAME_NOT_RESOLVED")
  - Die Adresse https://www.gesetze-im-internet.de ist erreichbar, aber verwendet das unnötige "www"-Präfix.

- SSL Issue
  - Es gibt ein "ServerCertificateName mismatch", denn juris.de != gesetze-im-internet.de
  - Das auf [gesetze-im-internet.de](https://www.gesetze-im-internet.de) gegebene Cert gilt für [juris.de](https://www.juris.de).

### XML

- Problem "fundstelle typ=amtlich"
  - Im XML wird für Fundstellen immer der Typ "amtlich" angegeben, obwohl im gesamten System keine anderen Typen existieren.
  - Die Typ-Angabe ist daher überflüssig.
  - Regex zum Nachweis: `(<fundstelle typ="(?!(amtlich)").*?>)`

