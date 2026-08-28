// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bgbl
{
    // Reine Parsing-Hilfsfunktionen (offline testbar, kein Netzwerkzugriff).
    // Das bgbl.de-XAVER-Backend liefert kompaktes JSON der Form
    // {"id":"...","did":"...","l":"<label>",...}.

    // Bezeichnung des Top-Level-Knotens "Bundesgesetzblatt Teil I/II" für einen Teil.
    std::string part_label(int part);

    // Wandelt ein Teil-Argument ("1", "I", "2", "II") in 1 oder 2 um.
    bool parse_part(std::string const & value, int& part);

    // Findet die "id" des Knotens, dessen Label (exakt oder Präfix) übereinstimmt.
    // Gibt einen leeren String zurück, wenn nicht gefunden.
    std::string find_item_id(std::string const & json, std::string const & label, bool exact = true);

    // Findet die "did" des Knotens, dessen Label (exakt oder Präfix) übereinstimmt.
    // Gibt einen leeren String zurück, wenn nicht gefunden.
    std::string find_item_did(std::string const & json, std::string const & label, bool exact = true);

    // Findet die Knoten-ID des Links "Komplette Ausgabe" innerhalb einer Ausgabe-Ansicht
    // (roher text.xav-Antwortkörper). Gibt einen leeren String zurück, wenn nicht gefunden.
    std::string find_komplette_node(std::string const & html);

    // Extrahiert die absolute media.xav-Download-URL aus einer PDF-Ansicht
    // (roher text.xav?skin=pdf-Antwortkörper). Gibt einen leeren String zurück, wenn
    // nicht gefunden. Behandelt HTML-escapete Und-Zeichen (&amp; -> &).
    std::string extract_media_url(std::string const & html);

    // Detaillierter Ausgabeneintrag mit Label (z. B. "Nr. 1 vom 23.05.1949").
    struct IssueEntry
    {
        int nr = 0;
        std::string id;
        std::string did;
        std::string label;
    };

    // Listet alle Ausgaben eines Jahres auf. Jeder Eintrag enthält die Ausgabennummer, die
    // Dokument-ID (`did`, verwendet als XAVER-`start`-Knoten) und die Knoten-ID
    // (`id`, verwendet als `tocid` beim Laden der Ausgabe-Ansicht).
    std::vector<IssueEntry> list_issues(std::string const & json);

    // Listet alle Ausgaben eines Jahres mit ihren vollständigen Labels auf.
    std::vector<IssueEntry> list_issue_entries(std::string const & json);

    // Online-Sitzung mit dem bgbl.de-XAVER-Backend.
    class Session
    {
    public:
        Session();
        ~Session();

        Session(Session const &)            = delete;
        Session& operator=(Session const &) = delete;
        Session(Session&&)                  = delete;
        Session& operator=(Session&&)       = delete;

        // GET einer URL ausführen und den Antwortkörper zurückgeben (wirft bei HTTP/Transportfehler).
        std::string get(std::string const & url);

        // GET einer URL ausführen und den Antwortkörper in eine Datei schreiben. Gibt bei Erfolg true zurück.
        bool download_file(std::string const & url, std::string const & filepath);

        // XAVER-Navigationshilfen auf höherer Ebene.
        std::string toclevel(std::string const & node_id);
        std::string issue_html(std::string const & issue_id, std::string const & issue_did);
        std::string pdf_html(std::string const & komplette_node);

    private:
        std::string escape(std::string const & value);

        class CurlHandle
        {
        public:
            CurlHandle();
            ~CurlHandle();

            CurlHandle(CurlHandle const &)            = delete;
            CurlHandle& operator=(CurlHandle const &) = delete;
            CurlHandle(CurlHandle&&)                  = delete;
            CurlHandle& operator=(CurlHandle&&)       = delete;

            void* get() const
            { return curl_; }
            explicit operator bool() const
            { return curl_ != nullptr; }

        private:
            void* curl_ = nullptr;
        };

        CurlHandle curl_;
        std::string buffer_;
    };

    // Lädt eine einzelne Ausgabe (BGBl Teil/Jahr/Nr.) als PDF nach out_dir herunter.
    // Gibt EXIT_SUCCESS oder EXIT_FAILURE zurück. Wirft std::runtime_error bei Fehlern.
    // `jobs` wird aus API-Symmetrie akzeptiert, aber eine einzelne Ausgabe ist stets seriell.
    int download_issue(int part, int year, int nr, std::string const & out_dir, bool verbose, int jobs = 1);

    // Lädt alle Ausgaben eines Jahres herunter. Bis zu `jobs` Ausgaben werden
    // parallel heruntergeladen (jeder Worker nutzt seine eigene Sitzung, da das
    // XAVER-media-URL-CSRF-Token an die Sitzung gebunden ist, die es auflöste).
    int download_year(int part, int year, std::string const & out_dir, bool verbose, int jobs = 1);

    // Lädt alle Ausgaben eines Jahresbereichs [from_year, to_year] herunter (inklusive).
    // Die kombinierte Menge der Ausgaben wird mit bis zu `jobs` parallelen Workern heruntergeladen.
    int download_range(int part, int from_year, int to_year, std::string const & out_dir, bool verbose, int jobs = 1);

    // Listet die verfügbaren Ausgaben eines Jahres (BGBl Teil/Jahr) auf stdout auf, damit der
    // Benutzer sehen kann, welche --nr-Werte gültig sind. Gibt EXIT_SUCCESS zurück.
    int download_toc(int part, int year, bool verbose);
} // namespace bgbl
