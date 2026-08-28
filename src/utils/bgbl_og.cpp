// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "utils/bgbl_og.h"

#include <curl/curl.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <iterator>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace bgbl::og
{
    namespace
    {
        // API-`kind`-Wert für einen BGBl-Teil (Teil I -> bgbl1, Teil II -> bgbl2).
        std::string kind_for_part(int const part)
        { return part == 2 ? "bgbl2" : "bgbl1"; }

        // Entfernt ein anhängendes "#fragment" (z. B. "#page=2") aus einer Media-URL, damit
        // die gesamte konsolidierte Ausgabe-PDF heruntergeladen wird, nicht nur eine einzelne Seite.
        std::string strip_fragment(std::string const & url)
        {
            auto const pos = url.find('#');
            return pos == std::string::npos ? url : url.substr(0, pos);
        }

        // Extrahiert Felder im Stil `"key": value` aus einem einzelnen Ergebnisobjekt.
        std::string field_int(std::string const & obj, std::string const & key, int& out)
        {
            std::regex const re(key + R"PAT(\s*:\s*(-?\d+)[\s,])PAT");
            std::smatch m;
            if (std::regex_search(obj, m, re)) {
                try {
                    out = std::stoi(m.str(1));
                    return m.str(1);
                } catch (...) {
                    // durchfallen
                }
            }
            return {};
        }

        std::string field_str(std::string const & obj, std::string const & key)
        {
            std::regex const re(key + R"PAT(\s*:\s*"((?:[^"\\]|\\.)*)")PAT");
            std::smatch m;
            if (std::regex_search(obj, m, re)) {
                return m.str(1);
            }
            return {};
        }

        // Ein einzelnes Download-Ziel (bereits zu URL + Ausgabepfad aufgelöst).
        struct FileTarget
        {
            std::string url;
            std::string output_path;
            int number = 0;
        };

        // Lädt eine Liste aufgelöster Dateien auf bis zu `jobs` Worker-Threads verteilt herunter.
        // Jeder Worker besitzt seine eigene Sitzung (libcurl-Handles sind nicht
        // threadübergreifend teilbar). Gibt EXIT_SUCCESS nur zurück, wenn jede Datei heruntergeladen wurde.
        int parallel_download_files(std::vector<FileTarget> const & targets, int jobs)
        {
            if (targets.empty()) {
                spdlog::warn("Keine Ausgaben zum Herunterladen gefunden.");
                return EXIT_SUCCESS;
            }

            jobs = std::max(jobs, 1);
            jobs = std::min(jobs, static_cast<int>(targets.size()));

            std::mutex failures_mutex;
            std::vector<std::string> failures;
            std::atomic<int> completed{0};
            std::atomic<int> failed{0};

            auto const worker = [&](std::size_t const start_index) {
                Session session;
                for (std::size_t i = start_index; i < targets.size(); i += static_cast<std::size_t>(jobs)) {
                    auto const & target = targets.at(i);
                    try {
                        spdlog::info("Lade {} herunter …", target.output_path);
                        if (session.download_file(target.url, target.output_path)) {
                            completed.fetch_add(1, std::memory_order_relaxed);
                        } else {
                            failed.fetch_add(1, std::memory_order_relaxed);
                            std::scoped_lock const lock(failures_mutex);
                            failures.push_back(target.output_path);
                        }
                    } catch (std::exception const & e) {
                        failed.fetch_add(1, std::memory_order_relaxed);
                        std::scoped_lock const lock(failures_mutex);
                        failures.push_back(target.output_path + ": " + e.what());
                    }
                }
            };

            std::vector<std::thread> threads;
            threads.reserve(static_cast<std::size_t>(jobs));
            for (int j = 0; j < jobs; ++j) {
                threads.emplace_back(worker, static_cast<std::size_t>(j));
            }
            for (auto& t : threads) {
                t.join();
            }

            if (!failures.empty()) {
                spdlog::error("{} von {} Downloads fehlgeschlagen:", failures.size(), targets.size());
                for (auto const & f : failures) {
                    spdlog::error("  - {}", f);
                }
                return EXIT_FAILURE;
            }

            spdlog::info("{} Downloads abgeschlossen.", completed.load(std::memory_order_relaxed));
            return EXIT_SUCCESS;
        }

        // Ordnet Teil/Jahr/Nr. dem Dateinamen auf der Platte zu (spiegelt das XAVER-Backend).
        std::string output_filename(int const part, int const year, int const nr)
        { return "bgbl" + std::to_string(part) + "_" + std::to_string(year) + "_Nr" + std::to_string(nr) + ".pdf"; }
    } // namespace

    std::string api_url(int const part, int const year, std::optional<int> const nr)
    {
        std::string url = std::string(API_BASE) + "?year=" + std::to_string(year) + "&kind=" + kind_for_part(part);
        if (nr) {
            url += "&number=" + std::to_string(*nr);
        }
        return url;
    }

    std::vector<PdfEntry> parse_entries(std::string const & json)
    {
        std::vector<PdfEntry> result;
        std::map<std::string, std::size_t> seen; // document_url -> Index im Ergebnis

        // Gleicht jedes Ergebnisobjekt bis einschließlich seines "title"-Feldes ab. Die
        // konsolidierten Felder, die wir benötigen (number, date, document_url, title), stehen
        // alle vor dem abschließenden "content"-Array, daher stoppen wir bewusst bei
        // "title" — das verhindert, dass verirrte Klammern innerhalb des (langen) Content-Texts
        // die Übereinstimmung zerstören. Facet-Objekte haben kein "document_url" und werden
        // daher ignoriert.
        std::regex const obj_re(R"OG(\{[^{}]*?"document_url"[^{}]*?"title"\s*:\s*"((?:[^"\\]|\\.)*)")OG");
        for (std::sregex_iterator it(json.begin(), json.end(), obj_re); it != std::sregex_iterator(); ++it) {
            std::string const obj = it->str(0);

            int number = 0;
            if (field_int(obj, R"("number")", number).empty()) {
                continue;
            }

            std::string const document_url = strip_fragment(field_str(obj, R"("document_url")"));
            if (document_url.empty()) {
                continue;
            }

            auto const existing = seen.find(document_url);
            if (existing != seen.end()) {
                continue; // dieselbe PDF bereits eingereiht (ein weiteres Gesetz in der Ausgabe)
            }

            PdfEntry entry;
            entry.number       = number;
            entry.document_url = document_url;
            entry.date         = field_str(obj, R"("date")");
            entry.title        = field_str(obj, R"("title")");
            seen.emplace(document_url, result.size());
            result.push_back(std::move(entry));
        }
        return result;
    }

    std::vector<PdfEntry> parse_issues(std::string const & json)
    {
        std::vector<PdfEntry> result;
        std::map<int, std::size_t> seen; // Ausgabennummer -> Index im Ergebnis

        std::regex const obj_re(R"OG(\{[^{}]*?"document_url"[^{}]*?"title"\s*:\s*"((?:[^"\\]|\\.)*)")OG");
        for (std::sregex_iterator it(json.begin(), json.end(), obj_re); it != std::sregex_iterator(); ++it) {
            std::string const obj = it->str(0);

            int number = 0;
            if (field_int(obj, R"("number")", number).empty()) {
                continue;
            }

            std::string const document_url = strip_fragment(field_str(obj, R"("document_url")"));
            if (document_url.empty()) {
                continue;
            }

            auto const existing = seen.find(number);
            if (existing != seen.end()) {
                continue; // diese Ausgabennummer bereits aufgelistet
            }

            PdfEntry entry;
            entry.number       = number;
            entry.document_url = document_url;
            entry.date         = field_str(obj, R"("date")");
            entry.title        = field_str(obj, R"("title")");
            seen.emplace(number, result.size());
            result.push_back(std::move(entry));
        }
        return result;
    }

    int download_issue(
        int const part, int const year, int const nr, std::string const & out_dir, bool const verbose, int const jobs)
    {
        (void)verbose;
        (void)jobs;
        Session session;

        std::string const json = session.get(api_url(part, year, nr));
        auto const entries     = parse_entries(json);
        if (entries.empty()) {
            throw std::runtime_error(
                "Ausgabe Nr. " + std::to_string(nr) + " im Jahrgang " + std::to_string(year) + " (Teil " +
                std::to_string(part) + ") nicht gefunden.");
        }

        std::filesystem::create_directories(out_dir);
        FileTarget const target{
            .url         = entries.front().document_url,
            .output_path = (std::filesystem::path(out_dir) / output_filename(part, year, nr)).string(),
            .number      = nr};
        spdlog::info("Lade BGBl Teil {} {} Nr. {} herunter …", part, year, nr);
        if (!session.download_file(target.url, target.output_path)) {
            throw std::runtime_error(
                "Download der PDF fehlgeschlagen (Teil " + std::to_string(part) + " " + std::to_string(year) + " Nr. " +
                std::to_string(nr) + ").");
        }
        return EXIT_SUCCESS;
    }

    int download_year(int const part, int const year, std::string const & out_dir, bool const verbose, int const jobs)
    {
        (void)verbose;
        Session session;

        std::string const json = session.get(api_url(part, year));
        auto const entries     = parse_entries(json);
        if (entries.empty()) {
            spdlog::warn("Keine Ausgaben im Jahrgang {} gefunden.", year);
            return EXIT_SUCCESS;
        }

        std::vector<FileTarget> targets;
        targets.reserve(entries.size());
        std::ranges::transform(entries, std::back_inserter(targets), [part, year, &out_dir](auto const & e) {
            return FileTarget{
                .url         = e.document_url,
                .output_path = (std::filesystem::path(out_dir) / output_filename(part, year, e.number)).string(),
                .number      = e.number};
        });

        spdlog::info(
            "Jahrgang {} (OffeneGesetze.de): {} Ausgabe(n) werden heruntergeladen ({} parallel).",
            year,
            targets.size(),
            jobs);
        std::filesystem::create_directories(out_dir);
        return parallel_download_files(targets, jobs);
    }

    int download_range(
        int const part,
        int const from_year,
        int const to_year,
        std::string const & out_dir,
        bool const verbose,
        int const jobs)
    {
        (void)verbose;
        if (to_year < from_year) {
            throw std::runtime_error("Ungültiger Jahrgänge-Bereich: from > to.");
        }

        Session session;
        std::vector<FileTarget> targets;
        for (int year = from_year; year <= to_year; ++year) {
            std::string const json = session.get(api_url(part, year));
            auto const entries     = parse_entries(json);
            if (entries.empty()) {
                spdlog::warn("Jahrgang {} nicht gefunden, übersprungen.", year);
                continue;
            }
            std::ranges::transform(entries, std::back_inserter(targets), [part, year, &out_dir](auto const & e) {
                return FileTarget{
                    .url         = e.document_url,
                    .output_path = (std::filesystem::path(out_dir) / output_filename(part, year, e.number)).string(),
                    .number      = e.number};
            });
        }

        if (targets.empty()) {
            spdlog::warn("Keine Ausgaben im Bereich {}–{} gefunden.", from_year, to_year);
            return EXIT_SUCCESS;
        }

        spdlog::info(
            "Bereich {}–{} (OffeneGesetze.de): {} Ausgabe(n) werden heruntergeladen ({} parallel).",
            from_year,
            to_year,
            targets.size(),
            jobs);
        std::filesystem::create_directories(out_dir);
        return parallel_download_files(targets, jobs);
    }

    int download_toc(int const part, int const year, bool const verbose)
    {
        (void)verbose;
        Session session;

        std::string const json = session.get(api_url(part, year));
        auto const entries     = parse_issues(json);
        if (entries.empty()) {
            std::cout << "Keine Ausgaben im Jahrgang " << year << " gefunden.\n";
            return EXIT_SUCCESS;
        }

        std::cout << "BGBl Teil " << part << " — Jahrgang " << year << " (OffeneGesetze.de)\n";
        std::cout << "Verfügbare Ausgaben (--nr=WERT):\n";
        for (auto const & entry : entries) {
            std::cout << "  " << entry.number << "\t" << entry.date.substr(0, 10) << "\t" << entry.title << "\n";
        }
        return EXIT_SUCCESS;
    }
} // namespace bgbl::og
