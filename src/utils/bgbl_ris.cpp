// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "utils/bgbl_ris.h"

#include <curl/curl.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace bgbl::ris
{
    namespace
    {
        // RIS-`agent`-Wert für einen BGBl-Teil (Teil I -> bgbl-1, Teil II -> bgbl-2).
        std::string agent_for_part(int const part)
        { return part == 2 ? "bgbl-2" : "bgbl-1"; }

        // Ordnet den römischen Teil-Marker ("I"/"II") aus dem Ausgabenamen 1/2 zu.
        int part_from_roman(std::string const & roman)
        { return roman == "II" ? 2 : 1; }

        // Extrahiert das ausbalancierte `{...}`-JSON-Objekt, das an Position `open`
        // beginnt (muss auf die öffnende Klammer zeigen). Gibt eine leere Zeichenkette zurück,
        // falls keine passende schließende Klammer gefunden wird.
        std::string extract_object(std::string const & s, std::size_t const open)
        {
            if (open >= s.size() || s[open] != '{') {
                return {};
            }
            int depth = 0;
            for (std::size_t i = open; i < s.size(); ++i) {
                char const c = s[i];
                if (c == '{') {
                    ++depth;
                } else if (c == '}') {
                    --depth;
                    if (depth == 0) {
                        return s.substr(open, i - open + 1);
                    }
                }
            }
            return {};
        }

        // Erfasst das erste `"key":"value"` (Zeichenkettenwert) innerhalb von `obj`.
        std::string field_str(std::string const & obj, std::string const & key)
        {
            std::regex const re(key + R"PAT(\s*:\s*"((?:[^"\\]|\\.)*)")PAT");
            std::smatch m;
            if (std::regex_search(obj, m, re)) {
                return m.str(1);
            }
            return {};
        }

        // Ein einzelnes aufgelöstes Download-Ziel.
        struct FileTarget
        {
            std::string url;
            std::string output_path;
            std::string label;
        };

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
                        spdlog::info("Lade {} herunter …", target.label);
                        if (session.download_file(target.url, target.output_path)) {
                            completed.fetch_add(1, std::memory_order_relaxed);
                        } else {
                            failed.fetch_add(1, std::memory_order_relaxed);
                            std::scoped_lock const lock(failures_mutex);
                            failures.push_back(target.label);
                        }
                    } catch (std::exception const & e) {
                        failed.fetch_add(1, std::memory_order_relaxed);
                        std::scoped_lock const lock(failures_mutex);
                        failures.push_back(target.label + ": " + e.what());
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

        std::string output_filename(int const part, int const year, std::string const & number)
        { return "bgbl" + std::to_string(part) + "_" + std::to_string(year) + "_Nr" + number + ".pdf"; }
    } // namespace

    std::string pdf_url(int const part, int const year, std::string const & number)
    {
        return "https://www.recht.bund.de/eli/bund/BGBl-" + std::to_string(part) + "/" + std::to_string(year) + "/" +
               number + "/regelungstext.pdf?__blob=publicationFile";
    }

    std::vector<IssueEntry> parse_page(std::string const & json, int const want_part)
    {
        std::vector<IssueEntry> result;
        std::map<std::string, std::size_t> seen; // "<year>|<number>" -> Index

        std::string const marker = R"("item":)";
        std::size_t pos          = 0;
        while ((pos = json.find(marker, pos)) != std::string::npos) {
            // die öffnende geschweifte Klammer des Item-Objekts (nach dem Marker) finden
            std::size_t const brace = json.find('{', pos + marker.size());
            if (brace == std::string::npos) {
                break;
            }
            std::string const obj = extract_object(json, brace);
            if (obj.empty()) {
                break;
            }

            // Ausgabe-Deskriptor: "BGBl I, 2026 201" (Nummer kann "67-2" sein)
            std::regex const issue_re(
                R"RX("isPartOf":\s*\{\s*"@type"\s*:\s*"PublicationIssue"\s*,\s*"name"\s*:\s*"BGBl ([^,]+), (\d{4}) ([^"]+)")RX");
            std::smatch im;
            if (!std::regex_search(obj, im, issue_re)) {
                pos = brace + 1;
                continue;
            }
            int const part = part_from_roman(im.str(1));
            if (part != want_part) {
                pos = brace + 1;
                continue;
            }
            int const year           = std::stoi(im.str(2));
            std::string const number = im.str(3);

            std::string const key = std::to_string(year) + "|" + number;
            bool const inserted   = seen.emplace(key, result.size()).second;
            if (!inserted) {
                pos = brace + 1;
                continue; // diese Ausgabe bereits vorhanden (ein weiteres Gesetz darin)
            }

            IssueEntry entry;
            entry.year   = year;
            entry.number = number;
            entry.title  = field_str(obj, R"("name")");
            entry.date   = field_str(obj, R"("datePublished")");
            if (entry.date.empty()) {
                entry.date = field_str(obj, R"("legislationDate")");
            }
            result.push_back(std::move(entry));

            pos = brace + 1;
        }
        return result;
    }

    std::string next_page(std::string const & json)
    {
        std::regex const re(R"RX("next"\s*:\s*"(/[^"]*)")RX");
        std::smatch m;
        if (std::regex_search(json, m, re)) {
            return m.str(1);
        }
        return {};
    }

    std::vector<IssueEntry> collect_issues(int const part)
    {
        std::vector<IssueEntry> result;
        std::map<std::string, std::size_t> seen; // "<year>|<number>" -> Index über Seiten hinweg

        std::string page = "/v1/legislation?agent=" + agent_for_part(part) + "&size=200&pageIndex=0";
        Session session;
        for (;;) {
            std::string const json = session.get(std::string(API_BASE) + page);
            for (auto& e : parse_page(json, part)) {
                std::string const key = std::to_string(e.year) + "|" + e.number;
                bool const inserted   = seen.emplace(key, result.size()).second;
                if (inserted) {
                    result.push_back(std::move(e));
                }
            }

            std::string const next = next_page(json);
            if (next.empty()) {
                break;
            }
            page = next;
        }
        return result;
    }

    int download_issue(
        int const part,
        int const year,
        std::string const & number,
        std::string const & out_dir,
        bool const verbose,
        int const jobs)
    {
        (void)verbose;
        (void)jobs;
        Session session;

        std::filesystem::create_directories(out_dir);
        std::string const url  = pdf_url(part, year, number);
        std::string const path = (std::filesystem::path(out_dir) / output_filename(part, year, number)).string();

        spdlog::info("Lade BGBl Teil {} {} Nr. {} herunter …", part, year, number);
        if (!session.download_file(url, path)) {
            throw std::runtime_error(
                "Download der PDF fehlgeschlagen (Teil " + std::to_string(part) + " " + std::to_string(year) + " Nr. " +
                number + ").");
        }
        return EXIT_SUCCESS;
    }

    int download_year(int const part, int const year, std::string const & out_dir, bool const verbose, int const jobs)
    {
        (void)verbose;
        auto const all = collect_issues(part);

        std::vector<FileTarget> targets;
        for (auto const & e : all) {
            if (e.year != year) {
                continue;
            }
            targets.push_back(
                FileTarget{
                    .url         = pdf_url(part, e.year, e.number),
                    .output_path = (std::filesystem::path(out_dir) / output_filename(part, e.year, e.number)).string(),
                    .label       = output_filename(part, e.year, e.number)});
        }

        if (targets.empty()) {
            spdlog::warn("Keine Ausgaben im Jahrgang {} gefunden.", year);
            return EXIT_SUCCESS;
        }

        spdlog::info(
            "Jahrgang {} (Bund RIS): {} Ausgabe(n) werden heruntergeladen ({} parallel).", year, targets.size(), jobs);
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

        auto const all = collect_issues(part);

        std::vector<FileTarget> targets;
        for (auto const & e : all) {
            if (e.year < from_year || e.year > to_year) {
                continue;
            }
            targets.push_back(
                FileTarget{
                    .url         = pdf_url(part, e.year, e.number),
                    .output_path = (std::filesystem::path(out_dir) / output_filename(part, e.year, e.number)).string(),
                    .label       = output_filename(part, e.year, e.number)});
        }

        if (targets.empty()) {
            spdlog::warn("Keine Ausgaben im Bereich {}–{} gefunden.", from_year, to_year);
            return EXIT_SUCCESS;
        }

        spdlog::info(
            "Bereich {}–{} (Bund RIS): {} Ausgabe(n) werden heruntergeladen ({} parallel).",
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
        auto const all = collect_issues(part);

        std::vector<IssueEntry> entries;
        std::ranges::copy_if(all, std::back_inserter(entries), [year](auto const & e) {
            return e.year == year;
        });

        if (entries.empty()) {
            std::cout << "Keine Ausgaben im Jahrgang " << year << " gefunden.\n";
            return EXIT_SUCCESS;
        }

        std::cout << "BGBl Teil " << part << " — Jahrgang " << year << " (Bund RIS)\n";
        std::cout << "Verfügbare Ausgaben (--nr=WERT):\n";
        for (auto const & entry : entries) {
            std::cout << "  " << entry.number << "\t" << entry.date.substr(0, 10) << "\t" << entry.title << "\n";
        }
        return EXIT_SUCCESS;
    }
} // namespace bgbl::ris
