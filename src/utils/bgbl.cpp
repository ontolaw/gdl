// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "utils/bgbl.h"

#include <curl/curl.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <algorithm>
#include <atomic>
#include <map>

#include "version.h"

namespace
{
    struct FileCloser
    {
        void operator()(FILE* fp) const
        {
            if (fp != nullptr) {
                std::fclose(fp); // NOLINT(cppcoreguidelines-owning-memory)
            }
        }
    };

    std::size_t write_string_callback(void* ptr, std::size_t size, std::size_t nmemb, void* userdata)
    {
        auto* buffer = static_cast<std::string*>(userdata);
        buffer->append(static_cast<char const *>(ptr), size * nmemb);
        return size * nmemb;
    }

    std::size_t write_file_callback(void const * ptr, std::size_t size, std::size_t nmemb, void* userdata)
    {
        auto* file = static_cast<FILE*>(userdata);
        return fwrite(ptr, size, nmemb, file);
    }

    // Ersetzt HTML-escapete Und-Zeichen (&amp;) durch einfache Und-Zeichen.
    std::string unescape_ampersand(std::string const & value)
    {
        std::string result;
        std::string::size_type pos = 0;
        for (;;) {
            auto const idx = value.find("&amp;", pos);
            if (idx == std::string::npos) {
                result += value.substr(pos);
                break;
            }
            result += value.substr(pos, idx - pos);
            result += '&';
            pos = idx + 5;
        }
        return result;
    }

    // Maskiert Regex-Metazeichen in einem String-Literal.
    std::string regex_escape(std::string const & value)
    {
        static constexpr std::string_view metacharacters = R"(.\+*?^$[]{}()|)";
        std::string result;
        result.reserve(value.size());
        for (char const c : value) {
            if (metacharacters.contains(c)) {
                result += '\\';
            }
            result += c;
        }
        return result;
    }

    // Eine einzelne herunterzuladende Ausgabe, mit bereits aufgelösten Navigationsknoten
    // (issue_id + issue_did). Diese seriell im Vorfeld aufzulösen, lässt die Worker
    // die übergeordnete Navigation überspringen; nur die sitzungsgebundene media-URL-Auflösung
    // (issue_html -> pdf_html -> media-URL) muss in der eigenen Sitzung des Workers laufen.
    struct IssueTarget
    {
        int year = 0;
        int nr   = 0;
        std::string issue_id;
        std::string issue_did;
    };

    // Lädt eine Ausgabe über ein bereits aufgelöstes Ziel in der gegebenen Sitzung herunter.
    // Gibt bei Erfolg true zurück, wirft std::runtime_error bei nicht behebbaren Fehlern.
    // Wenn `out_media_url` nicht null ist, empfängt es die aufgelöste media.xav-URL, damit
    // der Aufrufer XAVER-Fehlverlinkungen erkennen kann (siehe parallel_download).
    bool download_target(
        bgbl::Session& session,
        IssueTarget const & target,
        int const part,
        std::string const & out_dir,
        std::string* out_media_url = nullptr)
    {
        std::string const issue_view = session.issue_html(target.issue_id, target.issue_did);
        std::string const komplette  = bgbl::find_komplette_node(issue_view);
        if (komplette.empty()) {
            throw std::runtime_error(
                "Knoten 'Komplette Ausgabe' nicht gefunden (Teil " + std::to_string(part) + " " +
                std::to_string(target.year) + " Nr. " + std::to_string(target.nr) + ").");
        }

        std::string const pdf_view  = session.pdf_html(komplette);
        std::string const media_url = bgbl::extract_media_url(pdf_view);
        if (media_url.empty()) {
            throw std::runtime_error(
                "media.xav Download-URL nicht gefunden (Teil " + std::to_string(part) + " " +
                std::to_string(target.year) + " Nr. " + std::to_string(target.nr) + ").");
        }

        std::filesystem::path const output_path =
            std::filesystem::path(out_dir) / ("bgbl" + std::to_string(part) + "_" + std::to_string(target.year) +
                                              "_Nr" + std::to_string(target.nr) + ".pdf");
        spdlog::info("Lade BGBl Teil {} {} Nr. {} herunter …", part, target.year, target.nr);
        if (out_media_url != nullptr) {
            *out_media_url = media_url;
        }
        return session.download_file(media_url, output_path.string());
    }

    // Verteilt die Downloads auf `jobs` Worker-Threads. Jeder Worker besitzt seine
    // eigene Session (libcurl-Handles sind nicht threadübergreifend teilbar, und das
    // XAVER-media-URL-CSRF-Token ist sitzungsgebunden). Gibt EXIT_SUCCESS nur zurück, wenn
    // jedes Ziel erfolgreich heruntergeladen wurde.
    int parallel_download(
        int const part, std::vector<IssueTarget> const & targets, std::string const & out_dir, int jobs)
    {
        if (targets.empty()) {
            spdlog::warn("Keine Ausgaben zum Herunterladen gefunden.");
            return EXIT_SUCCESS;
        }

        jobs = std::max(jobs, 1);
        jobs = std::min(jobs, static_cast<int>(targets.size()));

        std::mutex failures_mutex;
        std::vector<std::string> failures;
        std::mutex resolved_mutex;
        std::vector<std::pair<std::string, std::string>> resolved; // "<year>_Nr<nr>" -> media_url
        std::atomic<int> completed{0};
        std::atomic<int> failed{0};

        auto const worker = [&](std::size_t const start_index) {
            bgbl::Session session;
            for (std::size_t i = start_index; i < targets.size(); i += static_cast<std::size_t>(jobs)) {
                auto const & target = targets.at(i);
                std::string media;
                try {
                    if (download_target(session, target, part, out_dir, &media)) {
                        completed.fetch_add(1, std::memory_order_relaxed);
                        std::scoped_lock const lock(resolved_mutex);
                        resolved.emplace_back(std::to_string(target.year) + "_Nr" + std::to_string(target.nr), media);
                    } else {
                        failed.fetch_add(1, std::memory_order_relaxed);
                        std::scoped_lock const lock(failures_mutex);
                        failures.push_back(
                            "Teil " + std::to_string(part) + " " + std::to_string(target.year) + " Nr. " +
                            std::to_string(target.nr));
                    }
                } catch (std::exception const & e) {
                    failed.fetch_add(1, std::memory_order_relaxed);
                    std::scoped_lock const lock(failures_mutex);
                    failures.push_back(
                        "Teil " + std::to_string(part) + " " + std::to_string(target.year) + " Nr. " +
                        std::to_string(target.nr) + ": " + e.what());
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

        // Guard: erkennt XAVER-Fehlverlinkungen, bei denen mehrere unterschiedliche Ausgaben
        // auf dasselbe zugrundeliegende PDF aufgelöst werden. Dies ist nicht fatal (wir haben
        // trotzdem gespeichert, was XAVER lieferte), entspricht aber genau dem Fehlermodus, der
        // still identische falsche Downloads z. B. für das gesamte Jahr 1949 erzeugte.
        {
            std::map<std::string, std::vector<std::string>> by_iid;
            std::regex const iid_re(R"(iid=([^&"\s]+))");
            for (auto const& [label, media] : resolved) {
                std::smatch m;
                std::string const iid = std::regex_search(media, m, iid_re) ? m.str(1) : std::string();
                by_iid[iid].push_back(label);
            }
            for (auto const& [iid, labels] : by_iid) {
                if (labels.size() > 1) {
                    std::string joined;
                    bool first = true;
                    for (auto const & label : labels) {
                        if (!first) {
                            joined += ", ";
                        }
                        joined += label;
                        first = false;
                    }
                    spdlog::warn(
                        "{} Ausgaben verweisen auf dieselbe PDF (media iid={}): {}",
                        labels.size(),
                        iid.empty() ? "<kein iid>" : iid,
                        joined);
                }
            }
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
} // namespace

namespace bgbl
{
    std::string part_label(int const part)
    { return part == 2 ? "Bundesgesetzblatt Teil II" : "Bundesgesetzblatt Teil I"; }

    bool parse_part(std::string const & value, int& part)
    {
        if (value == "1" || value == "I" || value == "i") {
            part = 1;
            return true;
        }
        if (value == "2" || value == "II" || value == "ii") {
            part = 2;
            return true;
        }
        return false;
    }

    std::string find_item_id(std::string const & json, std::string const & label, bool const exact)
    {
        std::string const pattern =
            R"bg("id":"([^"]+)","did":"[^"]*","l":")bg" + regex_escape(label) + (exact ? R"bg(")bg" : R"bg()bg");
        std::regex const re(pattern);
        std::smatch match;
        if (std::regex_search(json, match, re)) {
            return match.str(1);
        }
        return {};
    }

    std::string find_item_did(std::string const & json, std::string const & label, bool const exact)
    {
        std::string const pattern =
            R"bg("id":"[^"]+","did":"([^"]+)","l":")bg" + regex_escape(label) + (exact ? R"bg(")bg" : R"bg()bg");
        std::regex const re(pattern);
        std::smatch match;
        if (std::regex_search(json, match, re)) {
            return match.str(1);
        }
        return {};
    }

    std::string find_komplette_node(std::string const & html)
    {
        std::regex const re(R"bg(node_id%3D(?:%27|')(\d+)(?:%27|'))bg");
        std::string region = html;
        auto const pos     = html.find("Komplette Ausgabe");
        if (pos != std::string::npos) {
            region = html.substr(0, pos);
        }

        std::string last_match;
        for (std::sregex_iterator it(region.begin(), region.end(), re); it != std::sregex_iterator(); ++it) {
            last_match = (*it).str(1);
        }
        return last_match;
    }

    std::string extract_media_url(std::string const & html)
    {
        std::regex const re(R"(media\.xav/([^?\s"']+)\?([^"'\s<]+))");
        std::smatch match;
        if (std::regex_search(html, match, re)) {
            std::string relative = match.str(0);
            relative             = unescape_ampersand(relative);
            return "https://www.bgbl.de/xaver/bgbl/" + relative;
        }
        return {};
    }

    std::vector<IssueEntry> list_issues(std::string const & json)
    {
        std::vector<IssueEntry> result;
        std::regex const re(R"bg("id":"([^"]+)","did":"([^"]+)","l":"Nr\.\s*(\d+)\s+vom)bg");
        for (std::sregex_iterator it(json.begin(), json.end(), re); it != std::sregex_iterator(); ++it) {
            auto const & match = *it;
            IssueEntry entry;
            entry.id  = match.str(1);
            entry.did = match.str(2);
            try {
                entry.nr = std::stoi(match.str(3));
            } catch (...) {
                continue;
            }
            result.push_back(std::move(entry));
        }
        return result;
    }

    std::vector<IssueEntry> list_issue_entries(std::string const & json)
    {
        std::vector<IssueEntry> result;
        std::regex const re(R"bg("id":"([^"]+)","did":"([^"]+)","l":"(Nr\.\s*\d+\s+vom[^"]*)")bg");
        for (std::sregex_iterator it(json.begin(), json.end(), re); it != std::sregex_iterator(); ++it) {
            auto const & match = *it;
            IssueEntry entry;
            entry.id    = match.str(1);
            entry.did   = match.str(2);
            entry.label = match.str(3);
            // die Ausgabennummer aus dem Label "Nr. <n> vom ..." ableiten
            std::regex const nr_re(R"bg(Nr\.\s*(\d+))bg");
            std::smatch nr_match;
            if (std::regex_search(entry.label, nr_match, nr_re)) {
                try {
                    entry.nr = std::stoi(nr_match.str(1));
                } catch (...) {
                    entry.nr = 0;
                }
            }
            result.push_back(std::move(entry));
        }
        return result;
    }

    Session::CurlHandle::CurlHandle() : curl_(curl_easy_init())
    {
        // empty
    }

    Session::CurlHandle::~CurlHandle()
    {
        if (curl_ != nullptr) {
            curl_easy_cleanup(static_cast<CURL*>(curl_));
            curl_ = nullptr;
        }
    }

    Session::Session()
    {
        if (!curl_) {
            throw std::runtime_error("curl konnte nicht initialisiert werden.");
        }

        CURL* const curl = static_cast<CURL*>(curl_.get());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        // Die Zertifikatsketten-Validierung von bgbl.de ist unzuverlässig; spiegelt den
        // projektweiten Ansatz für gesetze-im-internet.de wider.
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        std::string const useragent = std::string("Ontolaw-GDL/") + app_version::get_version();
        curl_easy_setopt(curl, CURLOPT_USERAGENT, useragent.c_str());

        // Aktiviert den In-Memory-Cookie-Speicher, damit die Sitzung (JSESSIONID +
        // bgblxaver) über die Navigationsanfragen hinweg erhalten bleibt.
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
    }

    Session::~Session() = default;

    std::string Session::escape(std::string const & value)
    {
        CURL* const curl = static_cast<CURL*>(curl_.get());
        char* escaped    = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
        if (escaped == nullptr) {
            throw std::runtime_error("URL-Kodierung fehlgeschlagen.");
        }
        std::string result(escaped);
        curl_free(escaped);
        return result;
    }

    std::string Session::get(std::string const & url)
    {
        CURL* const curl = static_cast<CURL*>(curl_.get());
        buffer_.clear();
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer_);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string_callback);

        CURLcode const res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            throw std::runtime_error(
                std::string("bgbl GET fehlgeschlagen: ") + curl_easy_strerror(res) + " (" + url + ")");
        }
        return buffer_;
    }

    bool Session::download_file(std::string const & url, std::string const & filepath)
    {
        CURL* const curl = static_cast<CURL*>(curl_.get());

#if defined(_MSC_VER) && (defined(_WIN32) || defined(_WIN64))
        FILE* raw_file = nullptr;
        fopen_s(&raw_file, filepath.c_str(), "wb");
        std::unique_ptr<FILE, FileCloser> file(raw_file);
#else
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        std::unique_ptr<FILE, FileCloser> file(fopen(filepath.c_str(), "wb"));
#endif
        if (file == nullptr) {
            spdlog::error("{} konnte nicht zum Schreiben geöffnet werden.", filepath);
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file.get());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, reinterpret_cast<curl_write_callback>(write_file_callback));

        CURLcode const res = curl_easy_perform(curl);
        file.reset();
        if (res != CURLE_OK) {
            spdlog::error("Download von {} fehlgeschlagen: {}.", url, curl_easy_strerror(res));
            std::error_code ec;
            std::filesystem::remove(filepath, ec);
            return false;
        }
        return true;
    }

    std::string Session::toclevel(std::string const & node_id)
    { return get("https://www.bgbl.de/xaver/bgbl/ajax.xav?q=toclevel&n=" + escape(node_id)); }

    std::string Session::issue_html(std::string const & issue_id, std::string const & issue_did)
    {
        // Der offizielle Bundestag-bgbl_scraper verwendet die eigene Knoten-ID der Ausgabe als
        // `tocid` (nicht die übergeordnete Jahres-ID). Die Verwendung der Jahres-ID lässt XAVER
        // die Ausgabe-Ansicht für ältere Jahre falsch auflösen (z. B. zeigte das gesamte Jahr 1949
        // auf Nr. 9), daher spiegeln wir dieses Verhalten hier.
        std::string const start = "//*[@node_id='" + issue_did + "']";
        std::string const url =
            "https://www.bgbl.de/xaver/bgbl/text.xav?tf=xaver.component.Text_0&hlf="
            "xaver.component.Hitlist_0&tocid=" +
            escape(issue_id) + "&start=" + escape(start);
        return get(url);
    }

    std::string Session::pdf_html(std::string const & komplette_node)
    {
        std::string const start = "//*[@node_id='" + komplette_node + "']";
        std::string const url   = "https://www.bgbl.de/xaver/bgbl/text.xav?SID=&skin=pdf&start=" + escape(start) +
                                  "&origin=link&hlf=xaver.component.Hitlist_0&tf=xaver.component.Text_0";
        return get(url);
    }

    int download_issue(
        int const part, int const year, int const nr, std::string const & out_dir, bool const verbose, int const jobs)
    {
        (void)verbose;
        (void)jobs;
        Session session;

        std::string const root    = session.toclevel("0");
        std::string const teil_id = find_item_id(root, part_label(part));
        if (teil_id.empty()) {
            throw std::runtime_error("Bundesgesetzblatt Teil " + std::to_string(part) + " nicht gefunden.");
        }

        std::string const teil_json = session.toclevel(teil_id);
        std::string const year_id   = find_item_id(teil_json, std::to_string(year));
        if (year_id.empty()) {
            throw std::runtime_error("Jahrgang " + std::to_string(year) + " nicht gefunden.");
        }

        std::string const year_json = session.toclevel(year_id);
        std::string const issue_id  = find_item_id(year_json, "Nr. " + std::to_string(nr) + " vom", false);
        std::string const issue_did = find_item_did(year_json, "Nr. " + std::to_string(nr) + " vom", false);
        if (issue_id.empty() || issue_did.empty()) {
            throw std::runtime_error(
                "Ausgabe Nr. " + std::to_string(nr) + " im Jahrgang " + std::to_string(year) + " nicht gefunden.");
        }

        std::filesystem::create_directories(out_dir);
        IssueTarget const target{.year = year, .nr = nr, .issue_id = issue_id, .issue_did = issue_did};
        if (!download_target(session, target, part, out_dir)) {
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

        std::string const root    = session.toclevel("0");
        std::string const teil_id = find_item_id(root, part_label(part));
        if (teil_id.empty()) {
            throw std::runtime_error("Bundesgesetzblatt Teil " + std::to_string(part) + " nicht gefunden.");
        }

        std::string const teil_json = session.toclevel(teil_id);
        std::string const year_id   = find_item_id(teil_json, std::to_string(year));
        if (year_id.empty()) {
            throw std::runtime_error("Jahrgang " + std::to_string(year) + " nicht gefunden.");
        }

        std::string const year_json = session.toclevel(year_id);
        auto const issues           = list_issues(year_json);
        if (issues.empty()) {
            spdlog::warn("Keine Ausgaben im Jahrgang {} gefunden.", year);
            return EXIT_SUCCESS;
        }

        std::vector<IssueTarget> targets;
        targets.reserve(issues.size());
        std::ranges::transform(issues, std::back_inserter(targets), [year](auto const & e) {
            return IssueTarget{.year = year, .nr = e.nr, .issue_id = e.id, .issue_did = e.did};
        });

        spdlog::info("Jahrgang {}: {} Ausgabe(n) werden heruntergeladen ({} parallel).", year, targets.size(), jobs);
        std::filesystem::create_directories(out_dir);
        return parallel_download(part, targets, out_dir, jobs);
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
        std::string const root    = session.toclevel("0");
        std::string const teil_id = find_item_id(root, part_label(part));
        if (teil_id.empty()) {
            throw std::runtime_error("Bundesgesetzblatt Teil " + std::to_string(part) + " nicht gefunden.");
        }
        std::string const teil_json = session.toclevel(teil_id);

        std::vector<IssueTarget> targets;
        for (int year = from_year; year <= to_year; ++year) {
            std::string const year_id = find_item_id(teil_json, std::to_string(year));
            if (year_id.empty()) {
                spdlog::warn("Jahrgang {} nicht gefunden, übersprungen.", year);
                continue;
            }
            auto const issues = list_issues(session.toclevel(year_id));
            std::ranges::transform(issues, std::back_inserter(targets), [year](auto const & e) {
                return IssueTarget{.year = year, .nr = e.nr, .issue_id = e.id, .issue_did = e.did};
            });
        }

        if (targets.empty()) {
            spdlog::warn("Keine Ausgaben im Bereich {}–{} gefunden.", from_year, to_year);
            return EXIT_SUCCESS;
        }

        spdlog::info(
            "Bereich {}–{}: {} Ausgabe(n) werden heruntergeladen ({} parallel).",
            from_year,
            to_year,
            targets.size(),
            jobs);
        std::filesystem::create_directories(out_dir);
        return parallel_download(part, targets, out_dir, jobs);
    }

    int download_toc(int const part, int const year, bool const verbose)
    {
        (void)verbose;
        Session session;

        std::string const root    = session.toclevel("0");
        std::string const teil_id = find_item_id(root, part_label(part));
        if (teil_id.empty()) {
            throw std::runtime_error("Bundesgesetzblatt Teil " + std::to_string(part) + " nicht gefunden.");
        }

        std::string const teil_json = session.toclevel(teil_id);
        std::string const year_id   = find_item_id(teil_json, std::to_string(year));
        if (year_id.empty()) {
            throw std::runtime_error("Jahrgang " + std::to_string(year) + " nicht gefunden.");
        }

        std::string const year_json = session.toclevel(year_id);
        auto const entries          = list_issue_entries(year_json);
        if (entries.empty()) {
            std::cout << "Keine Ausgaben im Jahrgang " << year << " gefunden.\n";
            return EXIT_SUCCESS;
        }

        std::cout << "BGBl Teil " << part << " — Jahrgang " << year << "\n";
        std::cout << "Verfügbare Ausgaben (--nr=WERT):\n";
        for (auto const & entry : entries) {
            std::cout << "  " << entry.nr << "\t" << entry.label << "\n";
        }
        return EXIT_SUCCESS;
    }
} // namespace bgbl
