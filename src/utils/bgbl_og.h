// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "utils/bgbl.h"

namespace bgbl::og
{
    // Download-Backend für das Bundesgesetzblatt, basierend auf der offenen
    // OffeneGesetze.de-API (https://api.offenegesetze.de). Im Gegensatz zum bgbl.de-
    // XAVER-Backend liefert OffeneGesetze.de eine konsolidierte PDF pro
    // Ausgabenummer, sodass die Navigation eine einzelne REST-Abfrage statt einer
    // Sitzung mit XAVER-Knotenabfragen ist.

    // Basis-URL der OffeneGesetze.de-Veröffentlichungs-API.
    constexpr char const * API_BASE = "https://api.offenegesetze.de/v1/veroeffentlichung/";

    // Erstellt die API-Abfrage-URL für einen BGBl-Teil/Jahr (optional eine einzelne
    // Ausgabenummer). `kind` ist `bgbl1` für Teil I und `bgbl2` für Teil II.
    std::string api_url(int part, int year, std::optional<int> nr = std::nullopt);

    // Eine einzelne herunterladbare Ausgabe-PDF, wie von der API zurückgegeben.
    struct PdfEntry
    {
        int number = 0;
        std::string document_url; // absolute PDF-URL ohne das "#page="-Fragment
        std::string date;
        std::string title;
    };

    // Wandelt die API-JSON-Ergebnisliste in die enthaltenen Ausgabe-PDFs um. Mehrere
    // "Bestellungen" (Gesetze) innerhalb einer Ausgabe teilen sich dieselbe zugrundeliegende
    // PDF, daher wird die Ergebnisliste nach `document_url` entdupliziert.
    std::vector<PdfEntry> parse_entries(std::string const & json);

    // Wandelt die API-JSON-Ergebnisliste in eindeutige Ausgabenummern um (für die
    // Inhaltsverzeichnis-Auflistung). Jede Ausgabenummer behält Datum und Titel
    // ihres ersten Eintrags.
    std::vector<PdfEntry> parse_issues(std::string const & json);

    // Lädt eine einzelne Ausgabe (BGBl Teil/Jahr/Nr.) als PDF nach out_dir herunter.
    // Gibt EXIT_SUCCESS oder EXIT_FAILURE zurück. Wirft std::runtime_error bei Fehlern.
    int download_issue(int part, int year, int nr, std::string const & out_dir, bool verbose, int jobs = 1);

    // Lädt alle Ausgaben eines Jahres herunter. Bis zu `jobs` Ausgaben werden
    // parallel heruntergeladen.
    int download_year(int part, int year, std::string const & out_dir, bool verbose, int jobs = 1);

    // Lädt alle Ausgaben eines Jahresbereichs [from_year, to_year] herunter (inklusive)
    // mit bis zu `jobs` parallelen Workern.
    int download_range(int part, int from_year, int to_year, std::string const & out_dir, bool verbose, int jobs = 1);

    // Listet die verfügbaren Ausgaben eines Jahres (BGBl Teil/Jahr) auf stdout auf, damit der
    // Benutzer sehen kann, welche --nr-Werte gültig sind. Gibt EXIT_SUCCESS zurück.
    int download_toc(int part, int year, bool verbose);
} // namespace bgbl::og
