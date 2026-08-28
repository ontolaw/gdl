// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <string>
#include <vector>

#include "utils/bgbl.h"

namespace bgbl::ris
{
    // Download-Backend für das Bundesgesetzblatt, basierend auf dem offiziellen
    // Bundes-Rechtsinformationssystem (RIS), d. h. der Verkündungsplattform
    // recht.bund.de und dessen REST-API (rechtsinformationen.bund.de). Dies ist
    // der Nachfolger von OffeneGesetze.de und deckt die neueren (2023+) Jahrgänge ab.
    //
    // Im Gegensatz zum bgbl.de-XAVER-Backend ist die PDF-URL aus dem
    // ELI-Schema deterministisch, sodass eine einzelne Ausgabe ohne API-Navigation
    // heruntergeladen werden kann:
    //   https://www.recht.bund.de/eli/bund/BGBl-<TEIL>/<JAHR>/<NR>/regelungstext.pdf?__blob=publicationFile
    // Das Aufzählen aller Ausgaben eines Jahres erfordert weiterhin das
    // Paginieren der legislation-List-API.

    // Basis-URL der RIS-legislation-List-API.
    constexpr char const * API_BASE = "https://rechtsinformationen.bund.de";

    // Eine einzelne, über die API gefundene herunterladbare Ausgabe.
    struct IssueEntry
    {
        int year = 0;
        std::string number; // Ausgabennummer, kann ein ergänzendes Suffix enthalten ("67-2")
        std::string date;   // Veröffentlichungsdatum (YYYY-MM-DD)
        std::string title;  // Titel des ersten Gesetzes in der Ausgabe
    };

    // Erstellt die deterministische PDF-URL für eine einzelne Ausgabe. `number` ist die
    // im BGBl gedruckte Ausgabenummer (z. B. "201" oder ein ergänzendes "67-2").
    std::string pdf_url(int part, int year, std::string const & number);

    // Wandelt eine Hydra-Collections-Seite (JSON) in Ausgabeneinträge um, innerhalb
    // der Seite entdupliziert nach (year, number). Es werden nur Einträge zurückgegeben,
    // deren Teil `want_part` entspricht. Rein/offline-testbar.
    std::vector<IssueEntry> parse_page(std::string const & json, int want_part);

    // Extrahiert die relative "next"-Seiten-URL aus einem Hydra-`view`-Block oder eine
    // leere Zeichenkette, wenn keine weitere Seite existiert. Rein/offline-testbar.
    std::string next_page(std::string const & json);

    // Blättert durch die RIS-List-API für einen BGBl-Teil und sammelt alle Ausgaben,
    // entdupliziert nach (year, number). Gibt die vollständige Menge für den Teil
    // (alle Jahrgänge) zurück; Aufrufer filtern nach Jahr für toc/year/range.
    std::vector<IssueEntry> collect_issues(int part);

    // Lädt eine einzelne Ausgabe (BGBl Teil/Jahr/Nummer) als PDF nach out_dir herunter.
    // Gibt EXIT_SUCCESS oder EXIT_FAILURE zurück. Wirft std::runtime_error bei Fehlern.
    int download_issue(
        int part, int year, std::string const & number, std::string const & out_dir, bool verbose, int jobs = 1);

    // Lädt alle Ausgaben eines Jahres herunter. Bis zu `jobs` Ausgaben werden
    // parallel heruntergeladen.
    int download_year(int part, int year, std::string const & out_dir, bool verbose, int jobs = 1);

    // Lädt alle Ausgaben eines Jahresbereichs [from_year, to_year] herunter (inklusive)
    // mit bis zu `jobs` parallelen Workern.
    int download_range(int part, int from_year, int to_year, std::string const & out_dir, bool verbose, int jobs = 1);

    // Listet die verfügbaren Ausgaben eines Jahres (BGBl Teil/Jahr) auf stdout auf, damit der
    // Benutzer sehen kann, welche --nr-Werte gültig sind. Gibt EXIT_SUCCESS zurück.
    int download_toc(int part, int year, bool verbose);
} // namespace bgbl::ris
