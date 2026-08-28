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

`checksums.sha256` enthält SHA-256-Prüfsummen für Artefakte unter `data/gii` und `data/vvii`.

## Stabile Pfad-Identifikatoren für Gesetze

gdl benennt GII-Ordner aktuell nach dem URL-Slug (z.B. `1-dm-goldm_nzg`), der von
gesetze-im-internet.de vergeben wird und sich bei einer Umbenennung der Seite ändern
kann. Dadurch ist der Ordnerpfad nicht stabil: ein neuer Slug bricht Diff-, History-
und Container-Vergleiche, die auf unveränderlichen Pfaden aufbauen. Gesucht ist daher
ein kanonischer, unveränderlicher Identifikator je Gesetz/Vorschrift, der als stabiler
Pfadbestandteil dient.

Um einen solchen Identifikator zu finden, wurde mit `--download-limit=10` ein
Teildatensatz von 10 GII-Gesetzen heruntergeladen, entpackt und die extrahierten
XML-Dateien ausgewertet. Dabei zeigte sich, dass jede Gesetz-XML bereits nach ihrer
Dokumentnummer benannt ist (z.B. `BJNR204500000.xml`) und die `doknr` sowohl im
Wurzelelement `<dokumente doknr="BJNR…">` als auch auf jeder `<norm doknr="…">` trägt.

Der Suchweg verlief in zwei Schritten. Zuerst wurden die im XML verfügbaren
Kandidaten-Felder verglichen: der URL-Slug, die juristische Abkürzung `jurabk`, die
`doknr`, der Langtitel `langtitel` sowie die Fundstelle `fundstelle` (BGBl-I-Zitat).
Anschließend wurde geprüft, ob die `doknr`/`BJNR…`-Werte dokumentiert sind: es wurde
die öffentliche GII-Schema-DTD abgerufen sowie juris-Hilfeseiten und Drittprojekte
(OpenLegalData-Ingestor, die Python-Bibliothek `gesetze-im-internet`) durchsucht.

Gefunden wurde Folgendes: Die DTD `gii-norm.dtd` (Version 1.01, erstellt von juris
GmbH im Auftrag des Bundesministeriums der Justiz, öffentlich unter
`https://www.gesetze-im-internet.de/dtd/1.01/gii-norm.dtd`) deklariert `doknr` als
Attribut (`CDATA`, `#IMPLIED`) auf `<dokumente>` und `<norm>`. juris dokumentiert
`DOKNR` zudem als Suchfeld ("Suche nach Dokumenten mit einer bestimmten juris
Dokumentnummer"), und Drittprojekte verwenden `doknr` konsistent als kanonische
Quell-Identifikation. Nicht dokumentiert ist hingegen die interne Struktur des
`BJNR…`-Werts (Präfix plus Ziffernfolge) - der String ist ein undurchsichtiges,
juris-internes Code-Muster und muss daher als atomares Token behandelt werden.
Ergänzend existiert mit dem ELI-Permalink (`https://www.recht.bund.de/eli/bund/BGBl-1/{jahr}/{nr}/`)
ein offiziell spezifizierter, stabiler Identifikator der recht.bund.de-Plattform (verifiziert:
`.../2023/1/` wird mit HTTP 200 ausgeliefert). Allerdings verwendet dieser die fortlaufende
BGBl-Nummer pro Jahr - nicht die Seitenzahl aus `fundstelle` und nicht die `doknr` - und
ist daher nicht direkt aus den GII-Metadaten ableitbar, sondern benötigt eine externe
Zuordnung. Ob der ELI-Link historische, vor 2023 verkündete Gesetze (den Großteil des
GII-Bestands) abdeckt, ist ungeprüft (ein blind geratener `.../2000/1/` lieferte HTTP 404).

### Aufbau des `doknr` (BJNR-Identifikator)

Der Wert ist ein zusammengesetzter, von juris vergebener Code. Empirisch lässt sich
folgende Struktur beobachten:

- **Präfix (4 Buchstaben):** kennzeichnet den Dokumenttyp bzw. die Quelle. Für
  Bundesgesetze und -verordnungen lautet es `BJNR` (Bundesgesetzblatt). Im weiteren
  juris-System existieren weitere Präfixe für andere Dokumenttypen; `BJNR` ist für
  den GII-Bestand (Bundesrecht) der relevante Wert.
- **Numerischer Teil (9 Ziffern):** eine laufende, von juris vergebene
  Registriernummer. Eine offizielle Spezifikation dieser Ziffernfolge existiert
  nicht (siehe Recherche oben). Ein empirischer Abgleich von `doknr` und
  `fundstelle` aus dem eigenen 10-Gesetze-Download zeigt, dass sich darin **keine
  konsistente, entschlüsselbare Kodierung** von Jahr oder Seite befindet:

  | `doknr` | `fundstelle` (Jahr, Seite) | Seite im `doknr` erkennbar? |
  |---|---|---|
  | `BJNR204500000` | 2000, 2045 | ja (`2045` am Anfang) |
  | `BJNR002620970` | 1970, 262 | ja (`262` eingebettet) |
  | `BJNR014210976` | 1976, 1421 | ja (`1421` eingebettet) |
  | `BJNR002710954` | 1966, 292 | nein |
  | `BJNR508900008` | 2008, 2101 | nein |

  Die Seite taucht in einigen, aber nicht allen Fällen innerhalb der Ziffernfolge auf
  - vermutlich Zufall der Registrierungsreihenfolge, nicht beabsichtigte Kodierung.
  Eine Ableitung Zitation -> `doknr` ist daher nicht zuverlässig möglich.
- **Norm-Ebene:** Jede `<norm>` trägt ebenfalls ein `doknr`-Attribut, das aus der
  Dokument-`doknr` plus einem Suffix besteht, z.B. `BJNR204500000BJNE000102377`
  (Dokumentnummer + Suffix für die Einzelnorm). Die Norm-`doknr` enthält also die
  übergeordnete Dokument-`doknr` als Präfix und ist dieser hierarchisch
  untergeordnet.

**Fazit:** Der `BJNR…`-Wert ist ein undurchsichtiges, atomares Token. Er ist ein
hervorragender kanonischer Identifikator (eindeutig, quellstabil, schema-definiert),
aber seine internen Bestandteile dürfen nicht interpretiert oder zerlegt werden.

### Rückwärtssuche (von bekanntem Gesetz zur GII-Kennung)

Die umgekehrte Richtung - von einem bekannten Gesetz zu dessen GII-Identifikator -
wurde am Beispiel *"Betriebsordnung für Luftfahrtgerät vom 4. März 1970
(BGBl. I S. 262)"* erprobt:

- **Über den Titel (zuverlässig):** Die `gii-toc.xml` ist titel-indiziert. Eine
  Suche nach dem Titel liefert den Treffer `Betriebsordnung für Luftfahrtgerät` und
  daraus den URL-Slug **`luftbo`** (`https://www.gesetze-im-internet.de/luftbo/xml.zip`).
  Die zugehörige Gesetz-XML heißt `BJNR002620970.xml`, die root-`doknr` lautet
  `BJNR002620970`, die `jurabk` ist `LuftBO`.
- **Über die Fundstelle (verifizierend):** Die `fundstelle` der XML lautet
  `<periodikum>BGBl I</periodikum><zitstelle>1970, 262</zitstelle>` und deckt sich
  exakt mit der Angabe des Nutzers. Die `fundstelle` ist damit der Nachweis, dass der
  gefundene Datensatz tatsächlich das gesuchte Gesetz ist.
- **Nur über die Zitation (BGBl I S. 262) aus der TOC heraus: nicht möglich.** Die
  `gii-toc.xml` enthält nur `<title>` und `<link>`, nicht die `fundstelle`. Ein
  rein zitationsbasierter Zugriff benötigt daher entweder die `fundstelle` aus der
  bereits geladenen XML oder einen externen BGBl-Index.
- **Hinweis zur `doknr` (siehe auch Abschnitt "Aufbau des `doknr`"):** Die
  9-stellige Ziffernfolge ist eine undokumentierte, von juris vergebene
  Registriernummer. Die BGBl-Seite ist darin in einigen, aber nicht allen Fällen
  eingebettet (z.B. `262` in `BJNR002620970`, fehlend aber bei `BJNR002710954`
  für Seite 292). Eine verlässliche Ableitung Zitation -> `doknr` ist daher nicht
  möglich; die `doknr` ist als undurchsichtiges, atomares Token zu behandeln.

Der robuste Rückwärtsweg ist somit: **Titel -> TOC -> Slug -> `doknr`**, wobei die
`fundstelle` der XML den Treffer bestätigt. Die `doknr` bleibt auch hier der
kanonische Identifikator.

### Entwicklung eines eigenen Identifiers

Aus den vorigen Abschnitten folgt: der URL-Slug ist extern und instabil, `jurabk`
nicht eindeutig/stabil, `fundstelle` ändert sich bei Neufassung, der ELI-Link ist
plattformgebunden. Für den Dataset-Vertrag (Diff, Container, History) braucht gdl
daher einen **eigenen, stabilen Identifikator**, der unabhängig von externen
Umbenennungen bleibt.

**Entwurfsprinzipien:**

- **Stabil:** bleibt über Wiederholungen und Quelländerungen gleich.
- **Eindeutig:** eine Kennung pro Gesetz/Vorschrift.
- **Dateisystem-sicher:** ohne Zeichen, die Pfade gefährden (Leerzeichen, Umlaute
  in Pfaden vermeiden).
- **Aus vorhandenen Daten ableitbar:** nur Felder nutzen, die gdl ohne externe
  Dienste kennt (slug aus der TOC-URL, `doknr`/`jurabk`/`langtitel`/`fundstelle`
  aus der XML).
- **Trennung von Identität und Anzeige:** der maschinelle Schlüssel ist opak und
  unveränderlich; der menschenlesbare Name ist reine Metadaten.

**Vorgeschlagener Aufbau - Hybridmodell:**

- **Kanonischer Identifikator (Maschine): `doknr`.** Opak, quellstabil,
  schema-definiert, bereits im extrahierten Dateinamen vorhanden
  (`BJNR002620970.xml`). Er wird der Primärschlüssel in `manifest.json` und die
  Basis für Diff/Container/History. Da er nach dem Entpacken vorliegt, wird er im
  Post-Unzip-Schritt (bzw. `--format`/`--report`) zugewiesen.
- **Anzeigename (Mensch):** bereinigtes `jurabk` (oder `slug` als Fallback),
  gespeichert als Metadatum (`name`, `title`, `langtitel`, `slug` im Manifest).
  Er dient nur der Lesbarkeit und ist identitätsneutral.
- **Ordner/Pfad:** zwei Varianten sind möglich:
  - **(A) Maximale Stabilität:** Ordner = `doknr`
    (`raw-extracted/BJNR002620970/`). Ändert sich nie, opak für Menschen.
  - **(B) Lesbarkeit ("Ordner = Kürzel", wie im Roadmap 3.3 gewünscht):** Ordner =
    bereinigtes `jurabk` (`raw-extracted/LuftBO/`), mit `doknr` als kanonischem
    Schlüssel im Manifest. Hintergrund: wenn sich das Kürzel ändert, bleibt der
    Identitätsschlüssel (`doknr`) stabil, aber der *Pfad* selbst ändert sich - was
    Diff/Container bricht. Variante (B) ist daher nur lesbar, nicht pfad-stabil.
- **Empfehlung:** Variante **(A)** als kanonischer Dataset-Pfad (pfad-stabil),
  `jurabk`/`slug` als Anzeigename im Manifest. Für menschliche Navigation kann
  zusätzlich ein `index.json`/README je Gesetz den `doknr` -> Kürzel/Titel
  abbilden, ohne den Pfad zu beeinflussen.

**Manifest-Erweiterung (Vorschlag):**

```json
{
  "id": "BJNR002620970",
  "source": "gii",
  "name": "LuftBO",
  "title": "Betriebsordnung für Luftfahrtgerät",
  "slug": "luftbo",
  "fundstelle": "BGBl I 1970, 262"
}
```

**Zeitpunkt der Vergabe:** Da `doknr` erst nach dem Unzip bekannt ist, erfolgt die
Zuweisung nicht im Download-Schritt (der weiterhin `raw-zips/<slug>/` anlegt),
sondern beim Entpacken/Normalisieren. `try_get_file_and_folder` (`src/utils/download.cpp`)
bleibt für den Download unverändert; die kanonische Ordnerstruktur wird im
Post-Unzip-Schritt auf `doknr` umgestellt.

**Offen:** ob bei Variante (B) ein einmal vergebener Ordner bei Kürzel-Änderung
mitgeführt (Alias/History) oder hart umbenannt wird - hierfür ist das separate
Diff/History-Projekt zuständig.

### Document- vs. Publication-Identität (Versionsverlauf)

Ein Gesetz durchläuft im Bundesgesetzblatt (BGBl) eine Kette von Veröffentlichungen:
eine **Erstveröffentlichung**, danach **Änderungsgesetze** sowie gelegentlich
**Vollveröffentlichungen / Neubekanntmachungen** (z.B. eine "redaktionell
fehlerfreie Version"). Jede dieser Veröffentlichungen hat eine eigene
BGBl-Zitierung; die aktuelle konsolidierte Fassung ist das Ergebnis angewandter
Änderungen und kann bei einer Korrektur-Neuveröffentlichung faktisch eine "eigene"
Identität erhalten. Für einen stabilen Identifikator ist daher die Trennung zweier
Ebenen entscheidend:

- **Document-Identität:** das Gesetz als fortbestehendes Rechtsobjekt. In GII ist
  dies die `doknr` des Wurzelelements `<dokumente>` - sie kennzeichnet das
  konsolidierte Gesetz und bleibt über die Kette von Änderungsgesetzen hinweg gleich.
- **Publication-Identität:** die einzelne BGBl-Veröffentlichung (Erstveröffentlichung,
  Änderung, Vollveröffentlichung). Jede hat ihre eigene Zitierung (`fundstelle`,
  `geändert durch ...`) und ist im juris-Modell zum Teil ein eigenes, separates
  Dokument (eigene `doknr`).

Die GII-XML liefert zur Kette nur **Prosa-Text**, keine strukturierte `fundstelle`
je Änderung (belegt im 10-Gesetze-Download): `ausfertigung-datum` (Erstveröffentlichung,
z.B. `2000-12-27`), `standangabe` (aktueller Stand - 22 Fundstellen im Sample) sowie
`geändert durch ...` als Freitext in `<standkommentar>`, z.B.
`<standkommentar>Zuletzt geändert durch Art. 1 V v. 17.4.2024 I Nr. 130</standkommentar>`
(12 Vorkommen im Sample). Eine strukturierte `<fundstelle>` - wie sie für die
Erstveröffentlichung am Wurzelelement vorliegt (`<periodikum>BGBl I</periodikum>
<zitstelle>2000, 2045</zitstelle>`) - existiert für Änderungen **nicht**. Die
"exakte BGBl-Fundstelle" je Veröffentlichung ist also gerade NICHT in den GII-Daten
enthalten, sondern nur ihre Textform.

**Konsequenz für den Identifikator:**

- Die `doknr` identifiziert die **Document-Ebene** (das Gesetz) und ist damit der
  richtige kanonische Schlüssel: sie bleibt über Änderungsgesetze und die meisten
  Vollveröffentlichungen stabil, weil juris je Gesetz ein Dokument mit fester
  `doknr` führt. Die *Version* ist ein Zustand dieses Dokuments, abgebildet durch
  `standangabe`/`builddate` und den Inhalt (Prüfsumme im Manifest, siehe 3.2) - nicht
  durch eine neue `doknr`.
- **Datenlücke (Fundstelle):** Was für eine präzise Versions-/Diff-Kette eigentlich
  gebraucht wird - die exakte, strukturierte BGBl-Fundstelle (Quasi-Publikations-id,
  ggf. ELI-auflösbar) je Änderung - steckt nicht in den GII-XML. GII bietet nur den
  `standkommentar`-Prosa-Text ("geändert durch ..."). Die strukturierte Fundstelle
  (und damit ein stabiler Publikationsschlüssel) müsste aus einer externen Quelle
  erschlossen werden (amtlicher Fundstellennachweis, recht.bund.de, juris) - das
  liegt außerhalb von gdl (GII-only) und gehört in den Anreicherungsschritt des
  Downstream-/Diff-Projekts.
- **Restrisiko:** Eine Vollveröffentlichung/Neubekanntmachung kann im juris-Modell
  auch als *neues* Dokument mit *neuer* `doknr` geführt werden (Neubekanntmachung/
  Neuverkündung). Dann "wandert" das Gesetz unter eine neue `doknr`, und eine
  identitätsnaive Auswertung (gleiche `doknr` = gleiches Gesetz) bricht. Zudem
  wechselte das BGBl-Zitierformat (Seiten-basiert vor 2023, Nummern-basiert ab 2023,
  siehe recht.bund.de), wodurch auch Zitationen nicht stabil sind.
- **Mitigation:** Neben der `doknr` (Primärschlüssel) ist `jurabk`/`langtitel` als
  **sekundärer Match-Schlüssel** zu erfassen. Wenn eine `doknr` verschwindet und
  eine gleich benannte (`jurabk`/Titel) `doknr` auftaucht, ist dies als *Fortführung*
  (Alias) zu werten, nicht als neues Gesetz. Das eigentliche Alias-/History-Matching
  ist Sache des separaten Diff/History-Projekts; gdl muss die Daten (`doknr`,
  `jurabk`, `langtitel`, `fundstelle`, `standangabe`) im Manifest vollständig
  ausgeben, damit das Matching möglich wird.

**Fazit:** `doknr` = stabile Document-Identität; `standangabe`/`builddate` +
Prüfsumme = Version; `jurabk`/`langtitel` = Alias-Erkennung bei Re-Identifizierung.
Damit ist das Modell gegen das beschriebene Versionsverlaufs-Problem robust -
vorausgesetzt, das Downstream-Projekt behandelt Aliase.

### Optionen ohne External Enrichment

Die exakte, strukturierte BGBl-Fundstelle je Änderung steckt nicht in den GII-Daten
(siehe Datenlücke oben) und ließe sich nur mit externer Quelle erschließen. Für den
*Document-Identifikator* (roadmap 3.3) reicht jedoch das GII-eigene Datenmaterial.
Alle hier genannten Felder sind GII-nativ - es wird keine externe Anreicherung
benötigt:

- `doknr` (root `<dokumente>`, = extrahierter Dateiname) - schema-definiert, eindeutig,
  quellstabil
- `slug` (TOC `<link>`) - zum Download-Zeitpunkt gesetzt, extern gesteuert
- `jurabk` / `langteil` (Langtitel) - amtliche Abkürzung / Langtitel (änderbar)
- `ausfertigung-datum` - Datum der Erstveröffentlichung (relativ stabil)
- `fundstelle` - **nur** die Erstveröffentlichung, Format über die Zeit geändert
  (Seite -> Nummer)
- `standangabe` / `builddate` - aktueller Stand (ändert sich)
- `standkommentar` - Prosa-Text

**Möglichkeiten, davon abgeleitet:**

1. **`doknr` als kanonischer Identifikator** - die Lösung. Einziges Feld, das
   schema-definiert, eindeutig und über die normale Änderungskette stabil ist. Schon
   kostenlos als extrahierter Dateiname vorhanden. Schwäche: undurchsichtig sowie das
   seltene Re-Identifizierungs-Risiko (Vollveröffentlichung als neue `doknr`).
2. **`slug` <-> `doknr` Bindung** (beide GII-nativ: `slug` aus TOC-Link, `doknr` aus
   dem inneren Dateinamen der ZIP). Die Zuordnung im Manifest halten. Ändert sich der
   `slug`, verankert `doknr` die Identität; ändert sich `doknr`, signalisiert ein
   gleichgebliebener `slug` die Fortführung. Keine externe Quelle nötig.
3. **`jurabk` + `ausfertigung-datum` als sekundärer/Alias-Schlüssel** - beide
   GII-nativ, dient allein dazu zu *erkennen*, dass eine neue `doknr` faktisch dasselbe
   Gesetz ist (überbrückt das Re-Identifizierungs-Risiko aus 1).
4. **Composite `(doknr | jurabk + ausfertigung-datum)`** - primär `doknr`, Fallback
   auf die Kombination, wenn `doknr` fehlt oder offensichtlich gewechselt hat.

**Als kanonisch abgelehnt (GII-nativ, aber ungeeignet):**

- `slug` allein - extern gesteuert, kann umbenannt werden -> bricht Pfade.
- `fundstelle` - nur die *Erstveröffentlichung*, Format driftete (Seite -> Nummer),
  publications-bezogen statt document-bezogen.
- `langtitel` / `jurabk` allein - änderbar, nicht eindeutig garantiert.
- Inhalts-Hash - identifiziert den *Inhalt einer Version* (bereits in
  `checksums.sha256`), nicht das Gesetz über Versionen hinweg.

**Fazit:** `doknr` ist der einzig wahrhaft stabile, eindeutige, schema-definierte
Schlüssel aus GII; seine einzige Schwäche lässt sich vollständig mit *anderen
GII-nativen Feldern* (`slug`, `jurabk`, `ausfertigung-datum`) überbrücken - also
**ohne externe Anreicherung**. Nur die *publication-level* Fundstelle-Kette (exakte
Zitation je Änderung) bräuchte externe Anreicherung - ein feineres
Versionsproblem, nicht der 3.3-Identifikator selbst.

### Souveränitätsrisiko des `doknr`

`doknr` ist kein Identifikator des Bundesrechts, sondern ein Surrogat-Schlüssel in
der Datenbank von **juris GmbH** (private Tochter, LexisNexis/RELX). Die GII-DTD
wurde "im Auftrag des BMJ" von juris erstellt, und `DOKNR` heißt bei juris
ausdrücklich "juris Dokumentnummer". Die *amtlichen* Identifikatoren eines
Bundesgesetzes sind die **BGBl-Fundstelle** (`fundstelle`, z.B. `BGBl I 1970, 262`)
sowie zunehmend der **ELI**-Permalink (recht.bund.de) - diese sind hoheitlich/öffentlich.

Die GII-Daten stehen "zur freien Nutzung und Weiterverwendung", daher ist die
*Nutzung* des `doknr`-Werts als Schlüssel in einem eigenen Dataset rechtlich nicht
untersagt. Das *Schema* unterliegt aber einem privaten Anbieter: juris (bzw. das BMJ
als Auftraggeber) kann die Nummerierung jederzeit ändern oder den Betrieb
einstellen. `doknr` ist damit **entziehbar, nicht souverän und von einem privaten
Anbieter abhängig** - kein Garant für Langzeitstabilität.

**Konsequenz für den Entwurf:** `doknr` bleibt der praktische, GII-nativ verfügbare
Primärschlüssel, muss aber als **nicht-hoheitlicher Surrogat-Schlüssel** behandelt
werden. Zusammen mit `doknr` sind die *hoheitlichen* Anker - die originale
`fundstelle` sowie (wo auflösbar) der ELI-Permalink - im Manifest zu führen, damit
die Identität bei einer Änderung/Wegfall des `doknr` auf den amtlichen Identifikator
**re-anchored** werden kann. Langfristig ist der amtliche Identifikator (Fundstelle/
ELI) dem juris-Surrogat vorzuziehen; `doknr` ist der verfügbare Stopgap mit eingebauter
Fluchtlinie.

### Souveräne Alternative: Quelle BGBl / Verkündungsplattform

Die konsequente Antwort auf das Souveränitätsrisiko des `doknr` ist, die
*primäre, hoheitliche* Quelle zu nutzen: das **Bundesgesetzblatt** bzw. die
**Verkündungsplattform des Bundes** (`recht.bund.de`). Dort liegen die
Verkündungstexte und die amtlichen Identifikatoren (BGBl-Zitation, ELI) vor - nicht
das juris-Surrogat.

**Identifikatormodell (zwei Ebenen):**

1. **Publication (Verkündung):** BGBl-Zitation, z.B. `BGBl I 2023 Nr. 1`, entspricht
   dem ELI-Permalink `https://www.recht.bund.de/eli/bund/BGBl-1/2023/1/`. Hoheitlich,
   stabil, auflösbar, ELI-Standard. Dies ist der *publication-level* Identifikator,
   den GII nicht liefert.
2. **Konsolidiertes Dokument:** Ein Gesetz = Erstveröffentlichung + angewandte
   Änderungen, daher:
   - **Document-Anchor:** ELI der **Erstveröffentlichung** (z.B.
     `eli/bund/BGBl-1/1970/262` für die LuftBO) - fest, ändert sich nicht mit
     späteren Änderungen.
   - **Version:** ELI der Veröffentlichung, die den aktuellen konsolidierten Stand
     erzeugt hat (letzte Vollveröffentlichung/Neubekanntmachung).
   - **Diff-Kette:** geordnete Liste der Publication-ELIs - die *exakte Fundstelle-
     Kette*, hier aus der Primärquelle nativ verfügbar (die Änderungs-XML zitiert das
     geänderte Gesetz, daher ist Verkettung möglich).
   - Optional ein normalisierter Eigen-Id vom Citation-Typ: `BGBlI-1970-262`.

**Drei harte Einschränkungen (warum GII existiert):**

- **Historie-Lücke:** `recht.bund.de` ist das *elektronische BGBl ab 2023*. Ein
  Blindtest lieferte `.../2023/1/` -> HTTP 200, `.../2000/1/` -> HTTP 404. Vor-2023-
  (Papier-)Gesetze fehlen dort weitgehend - das Gesamtrecht lässt sich also nicht
  vollständig aus dem BGBl beziehen; GII hält die historischen konsolidierten Texte.
- **Konsolidierung ist der Aufwand:** Das BGBl liefert *Verkündungstexte* (die
  Änderungsgesetze), **nicht** das konsolidierte Ergebnis. Um "das Gesetz" neu
  aufzubauen, müssten alle Änderungen in Reihenfolge angewandt werden
  (Einfügungen, Aufhebungen, Neunummerierungen) - eine eigenständige Merge-Engine,
  die juris/BMJ für GII bereits betreiben.
- **Pragmatischer Hybrid:** Realistisch wird GII nicht ersetzt, sondern für den
  konsolidierten Text genutzt und alles auf **hoheitliche BGBl/ELI-Identifikatoren**
  umgeschüsselt (via der `fundstelle`, die GII für die Erstveröffentlichung bereits
  mitführt; Kreuzanreicherung für die Änderungskette, wo Abdeckung besteht). Das
  liefert souveräne Ids ohne eigene Konsolidierungs-Engine.

**Fazit:** Quelle BGBl/Verkündungsplattform ist die souveräne Antwort und ergibt
saubere ELI-basierte Identifikatoren plus echte Versionskette - bezahlt aber mit
Historie-Lücken und einer Konsolidierungs-Engine, die neu zu bauen wäre.

## Scope von gdl (Abgrenzung)

gdl ist ausschließlich ein Download- und Aufbereitungstool: es lädt den
Datenbestand einer Quelle (GII/VVII) herunter, entpackt, formatiert die XML und
erzeugt einen Datensatz-Report. Das Ergebnis ist ein reproduzierbarer
Rohdaten-Ordner (Single Snapshot) unter dem Output-Root.

Zu diesem Ordner gehören als Snapshot-Metadaten zwingend `manifest.json` und
`checksums.sha256`. gdl erzeugt beide selbst, sie machen den Ordner
self-contained und beschreiben den Datenstand.

Was gdl bewusst nicht macht:
- **Container-Packaging & Publishing** erfolgt in CI (z.B. `tar.zst`/OCI pro
  Kalenderwoche), nicht in gdl.
- **Diff-, History- und semantisches Diffing** ist Gegenstand eines eigenen,
  separaten Projekts (eigenes Repository), das auf den publizierten, immutable
  Containern arbeitet.

Der von gdl erzeugte, deterministische Ordner ist die **Dataset-Spec**. Der
Vertrag zwischen gdl und allen Downstream-Consumern (CI, Diff-Tool). gdl muss
ihn daher reproduzierbar erzeugen (kanonisches XML, stable Pfade), damit
Containervergleiche nicht durch Formatierungs-Rauschen verfälscht werden.

## VVII-Crawl & Resumability

Der VVII-Crawl (`crawl_vvii_from_toc` in `utils/crawler.cpp`) ist resistent
gegen Abbruch: er schreibt seinen Fortschritt in `state/` unter dem Output-Root
und setzt bei einem erneuten Lauf nahtlos fort.

- `state/frontier-pages.txt` — noch zu besuchende Seiten.
- `state/visited-pages.txt` — bereits gecrawlte Seiten.
- `state/discovered-artifacts.txt` — gefundene Artefakt-URLs.
- `state/downloaded-artifacts.txt` — erfolgreich heruntergeladene Artefakte.
- `state/failed-artifacts.txt` — fehlgeschlagene Artefakte.

Bereits existierende Seiten (`raw-pages/`) und Artefakte (`raw-artifacts/`)
werden beim erneuten Lauf nicht erneut geladen. Ein unterbrochener Crawl kann
daher einfach wiederholt werden, ohne doppelte Downloads zu erzeugen. Die
`dataset-report`-Auswertung liest diese State-Dateien aus und berichtet die
jeweiligen Anzahlen.

## Downloads für Tests begrenzen (Testmodus)

Ein vollständiger Lauf lädt den gesamten Datenbestand (GII: mehrere tausend ZIP-Dateien,
VVII: viele HTML-Seiten plus Artefakte). Für die Entwicklung und zum Testen der
nachgelagerten Schritte (Unzip, Format, Report) ist das unnötig und langsam.

Über den versteckten globalen Parameter `--download-limit=N` kann die Anzahl der
heruntergeladenen Dateien auf `1–10` begrenzt werden:

```sh
gdl --download-limit=3 -t -d
```

Der Parameter gilt für beide Quellen:

- **GII** (`--source=gii`): Es werden nur die ersten `N` ZIP-URLs aus der
  `gii-toc.xml` verarbeitet (`cmd.download-files.cpp`, `urls.resize(limit)`).
- **VVII** (`--source=vvii`): Der Crawl wird auf die ersten `N` TOC-Seiten
  beschränkt (`crawl_vvii_from_toc` in `utils/crawler.cpp`).

Werte außerhalb von `1–10` werden mit einem Fehler abgewiesen. Der Parameter ist
bewusst als Test-Hilfsmittel („versteckter Testmodus") gedacht, nicht als
allgemeines Limit.

### End-to-End mit wenigen Dateien testen

Da die Schritte Unzip, Format und Report *verzeichnisbasiert* arbeiten
(`raw-zips/`, `raw-extracted/`), verarbeiten sie automatisch nur die vorhandenen
Dateien. Nach einem limitierten Download genügt daher die normale Kette:

```sh
gdl -t -d --download-limit=3 -u -f -r
```

1. `-t` (`--toc`) lädt das Inhaltsverzeichnis.
2. `-d` (`--download`) lädt nur `3` Dateien nach `raw-zips/`.
3. `-u` (`--unzip`) entpackt die vorhandenen ZIPs nach `raw-extracted/`.
4. `-f` (`--format`) formatiert die vorhandenen XML-Dateien.
5. `-r` (`--report`) erzeugt den `datensatz-bericht.md` aus den vorhandenen Dateien.

So lässt sich der gesamte Ablauf ohne Voll-Download schnell und lokal prüfen.

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

Alle externen Libraries werden über vcpkg eingebunden.

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

## TODO (geplant / nicht implementiert)

Die folgenden Artefakte sind im Dataset-Layout vorgesehen, aber aktuell noch
**nicht implementiert**. Sie dienen der geplanten Auto-Commit-/Diff- und
Änderungshistorie-Workflow (siehe README-TODO).

### `manifest.json`

Metadaten-Datei des Snapshots am Output-Root. Sie beschreibt den erzeugten
Datenstand und dient der maschinellen Weiterverarbeitung, Reproduzierbarkeit und
als Grundlage für Vergleiche zwischen Läufen (Diff/Änderungshistorie).

Verwendungszweck:

- **Lauf-Identifikation:** `run_timestamp` (UTC, ISO-8601) und `tool_version`
  machen jeden Snapshot nachvollziehbar und versionierbar.
- **Herkunft:** `source_urls` (inkl. TOC-URL) dokumentiert, woher die Daten stammen.
- **Umfang:** `file_counts` (`raw_zips`, `raw_extracted_xml`) liefert schnelle
  Kennzahlen über den Umfang ohne die Dateien scannen zu müssen.
- **Integrität:** `checksum_algorithm` (`sha256`) verknüpft das Manifest mit den
  Prüfsummen in `checksums.sha256`.
- **Diff/History:** Durch Gegenüberstellung zweier Manifeste lassen sich neu
  hinzugefügte, geänderte und entfernte Dateien bestimmen.

Mindest-Schema:

- `run_timestamp` (UTC, ISO-8601)
- `source_urls` (inkl. TOC URL)
- `tool_version`
- `file_counts` (`raw_zips`, `raw_extracted_xml`)
- `checksum_algorithm` (`sha256`)

### `checksums.sha256`

Prüfsummen-Datei am Output-Root mit SHA-256-Hashes für die Artefakte unter
`data/gii` und `data/vvii` (ZIPs, extrahierte XMLs, VVII-Artefakte).

Verwendungszweck:

- **Integritätsprüfung:** Erkennen korrupter oder unvollständig heruntergeladener
  Dateien beim erneuten Einlesen eines Snapshots.
- **Änderungserkennung:** Ein Abgleich der Hashes zwischen zwei Läufen zeigt
  exakt, welche Dateien sich inhaltlich geändert haben (unabhängig vom
  Dateinamen oder Zeitstempel).
- **Grundlage der Diff-Historie:** Die im README geplante tägliche/wöchentliche
  Änderungshistorie (neu/geändert/entfernt) basiert auf diesen Prüfsummen.
- **Reproduzierbarkeit:** Hashes erlauben den Nachweis, dass ein Snapshot
  unverändert archiviert/committet wurde.
