// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "utils/crawler.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iterator>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "utils/download.h"
#include "utils/files.h"
#include "utils/xml.h"

namespace
{
    struct UrlParts
    {
        std::string scheme;
        std::string host;
        std::string path;
        std::string query;
    };

    std::string to_lower(std::string value)
    {
        std::ranges::transform(value, value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    bool starts_with(std::string const & value, std::string const & prefix)
    { return value.starts_with(prefix); }

    std::string host_without_port(std::string const & host)
    {
        auto const colon_pos = host.find(':');
        if (colon_pos == std::string::npos) {
            return host;
        }
        return host.substr(0, colon_pos);
    }

    bool parse_url(std::string const & url, UrlParts& out)
    {
        auto const scheme_pos = url.find("://");
        if (scheme_pos == std::string::npos) {
            return false;
        }

        out.scheme = to_lower(url.substr(0, scheme_pos));

        std::size_t const host_start  = scheme_pos + 3;
        std::size_t const path_start  = url.find('/', host_start);
        std::size_t const query_start = url.find('?', host_start);

        if (path_start == std::string::npos) {
            if (query_start == std::string::npos) {
                out.host = url.substr(host_start);
                out.path = "/";
                out.query.clear();
                return !out.host.empty();
            }
            out.host  = url.substr(host_start, query_start - host_start);
            out.path  = "/";
            out.query = url.substr(query_start + 1);
            return !out.host.empty();
        }

        if (query_start == std::string::npos || query_start < path_start) {
            out.host = url.substr(host_start, path_start - host_start);
            out.path = url.substr(path_start);
            out.query.clear();
            return !out.host.empty();
        }

        out.host  = url.substr(host_start, path_start - host_start);
        out.path  = url.substr(path_start, query_start - path_start);
        out.query = url.substr(query_start + 1);
        return !out.host.empty();
    }

    std::string base_directory_of_path(std::string const & path)
    {
        auto const slash_pos = path.find_last_of('/');
        if (slash_pos == std::string::npos) {
            return "/";
        }
        return path.substr(0, slash_pos + 1);
    }

    std::string resolve_url(std::string const & baseUrl, std::string const & candidate)
    {
        if (candidate.empty()) {
            return {};
        }

        auto const candidate_lower = to_lower(candidate);
        if (starts_with(candidate_lower, "javascript:") || starts_with(candidate_lower, "mailto:") ||
            starts_with(candidate_lower, "tel:") || starts_with(candidate_lower, "data:") ||
            starts_with(candidate_lower, "#")) {
            return {};
        }

        if (starts_with(candidate_lower, "http://") || starts_with(candidate_lower, "https://")) {
            return candidate;
        }

        UrlParts base;
        if (!parse_url(baseUrl, base)) {
            return {};
        }

        if (starts_with(candidate, "//")) {
            return base.scheme + ":" + candidate;
        }

        if (starts_with(candidate, "/")) {
            return base.scheme + "://" + base.host + candidate;
        }

        return base.scheme + "://" + base.host + base_directory_of_path(base.path) + candidate;
    }

    bool is_allowed_vvii_host(std::string const & host)
    {
        auto const host_lower = to_lower(host_without_port(host));
        return host_lower == "verwaltungsvorschriften-im-internet.de" ||
               host_lower == "www.verwaltungsvorschriften-im-internet.de" || host_lower == "localhost" ||
               host_lower == "127.0.0.1";
    }

    bool has_allowed_artifact_extension(std::string const & url)
    {
        auto const lower                                    = to_lower(url);
        static std::array<char const *, 5> const extensions = {".pdf", ".doc", ".docx", ".zip", ".xml"};
        return std::ranges::any_of(extensions, [&lower](char const * extension) {
            auto const pos = lower.find(extension);
            if (pos == std::string::npos) {
                return false;
            }

            std::size_t const end = pos + std::strlen(extension);
            return end == lower.size() || lower.at(end) == '?' || lower.at(end) == '#';
        });
    }

    std::vector<std::string> extract_href_like_values(std::string const & html)
    {
        std::vector<std::string> values;
        std::regex const re(R"((?:href|src)\s*=\s*["']([^"']+)["'])", std::regex_constants::icase);

        auto begin = std::sregex_iterator(html.begin(), html.end(), re);
        auto end   = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            values.push_back((*it).str(1));
        }

        return values;
    }

    std::string sanitize_segment(std::string const & segment)
    {
        std::string result;
        result.reserve(segment.size());

        for (char const ch : segment) {
            bool const ok = std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '-' || ch == '_' || ch == '.';
            result.push_back(ok ? ch : '_');
        }

        if (result.empty()) {
            return "_";
        }

        return result;
    }

    std::filesystem::path url_to_storage_path(
        std::filesystem::path const & root, std::string const & url, std::string const & fallbackFileExtension)
    {
        UrlParts parts;
        if (!parse_url(url, parts)) {
            auto const hashed = std::to_string(std::hash<std::string>{}(url));
            return root / ("invalid_" + hashed + fallbackFileExtension);
        }

        std::filesystem::path relative;
        relative /= sanitize_segment(host_without_port(parts.host));

        std::string path = parts.path.empty() ? "/" : parts.path;
        if (path.front() == '/') {
            path.erase(path.begin());
        }

        std::stringstream path_stream(path);
        std::string item;
        std::vector<std::string> segments;
        while (std::getline(path_stream, item, '/')) {
            if (item.empty()) {
                continue;
            }
            segments.push_back(sanitize_segment(item));
        }

        if (segments.empty()) {
            segments.push_back("index" + fallbackFileExtension);
        }

        for (std::size_t i = 0; i < segments.size(); ++i) {
            if (i + 1 == segments.size()) {
                break;
            }
            relative /= segments.at(i);
        }

        std::string filename = segments.back();
        if (!filename.contains('.')) {
            filename += fallbackFileExtension;
        }

        if (!parts.query.empty()) {
            auto const query_hash = std::to_string(std::hash<std::string>{}(parts.query));
            auto const dot_pos    = filename.find_last_of('.');
            if (dot_pos != std::string::npos) {
                filename.insert(dot_pos, "__q" + query_hash.substr(0, 8));
            } else {
                filename += "__q" + query_hash.substr(0, 8);
            }
        }

        relative /= filename;
        return root / relative;
    }

    std::unordered_set<std::string> load_set(std::filesystem::path const & filepath)
    {
        std::unordered_set<std::string> values;
        if (!std::filesystem::exists(filepath)) {
            return values;
        }

        std::ifstream in(filepath.string());
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty()) {
                values.insert(line);
            }
        }

        return values;
    }

    void append_lines(std::filesystem::path const & filepath, std::vector<std::string> const & lines)
    {
        if (lines.empty()) {
            return;
        }

        std::ofstream out(filepath.string(), std::ios::app);
        for (auto const & line : lines) {
            out << line << "\n";
        }
    }

    std::vector<std::string> collect_artifact_urls_from_page(
        std::filesystem::path const & htmlFile, std::string const & pageUrl)
    {
        std::vector<std::string> artifacts;

        std::ifstream in(htmlFile.string());
        if (!in) {
            return artifacts;
        }

        std::string const html((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        auto const links = extract_href_like_values(html);

        std::set<std::string> dedup;
        for (auto const & link : links) {
            auto const absolute = resolve_url(pageUrl, link);
            if (absolute.empty()) {
                continue;
            }

            UrlParts parts;
            if (!parse_url(absolute, parts)) {
                continue;
            }

            if (!is_allowed_vvii_host(parts.host)) {
                continue;
            }

            if (!has_allowed_artifact_extension(absolute)) {
                continue;
            }

            dedup.insert(absolute);
        }

        artifacts.assign(dedup.begin(), dedup.end());
        return artifacts;
    }
} // namespace

int crawl_vvii_from_toc(
    std::filesystem::path const & tocFilepath,
    std::filesystem::path const & outputRoot,
    bool const curlVerbose,
    std::optional<int> const pageLimit)
{
    auto const urls = get_urls(tocFilepath.string());
    if (urls.empty()) {
        spdlog::warn("Keine URLs in vvii TOC gefunden: {}", tocFilepath.string());
        return EXIT_SUCCESS;
    }

    std::vector<std::string> page_urls = urls;
    if (pageLimit.has_value() && pageLimit.value() > 0 &&
        page_urls.size() > static_cast<std::size_t>(pageLimit.value())) {
        page_urls.resize(static_cast<std::size_t>(pageLimit.value()));
        spdlog::info("Testlimit aktiv für vvii crawl: {} Seiten.", pageLimit.value());
    }

    auto const raw_pages_root     = outputRoot / "raw-pages";
    auto const raw_artifacts_root = outputRoot / "raw-artifacts";
    auto const state_root         = outputRoot / "state";

    std::filesystem::create_directories(raw_pages_root);
    std::filesystem::create_directories(raw_artifacts_root);
    std::filesystem::create_directories(state_root);

    auto const state_frontier   = state_root / "frontier-pages.txt";
    auto const state_visited    = state_root / "visited-pages.txt";
    auto const state_discovered = state_root / "discovered-artifacts.txt";
    auto const state_downloaded = state_root / "downloaded-artifacts.txt";
    auto const state_failed     = state_root / "failed-artifacts.txt";

    append_lines(state_frontier, page_urls);

    auto visited_pages        = load_set(state_visited);
    auto downloaded_artifacts = load_set(state_downloaded);

    std::unordered_set<std::string> all_discovered_artifacts;
    int page_download_failures = 0;

    for (auto const & page_url : page_urls) {
        auto const page_target = url_to_storage_path(raw_pages_root, page_url, ".html");
        std::filesystem::create_directories(page_target.parent_path());

        if (!std::filesystem::exists(page_target)) {
            if (download_file(page_url, page_target.string(), curlVerbose) != EXIT_SUCCESS) {
                spdlog::error("VVII Seite konnte nicht geladen werden: {}", page_url);
                page_download_failures++;
                continue;
            }
        }

        visited_pages.insert(page_url);
        auto const artifacts = collect_artifact_urls_from_page(page_target, page_url);
        for (auto const & artifact_url : artifacts) {
            all_discovered_artifacts.insert(artifact_url);
        }
    }

    append_lines(state_visited, std::vector<std::string>(visited_pages.begin(), visited_pages.end()));
    append_lines(
        state_discovered, std::vector<std::string>(all_discovered_artifacts.begin(), all_discovered_artifacts.end()));

    int artifact_downloads = 0;
    int artifact_failures  = 0;

    for (auto const & artifact_url : all_discovered_artifacts) {
        if (downloaded_artifacts.contains(artifact_url)) {
            continue;
        }

        auto const artifact_target = url_to_storage_path(raw_artifacts_root, artifact_url, ".bin");
        std::filesystem::create_directories(artifact_target.parent_path());

        int const result = download_file(artifact_url, artifact_target.string(), curlVerbose);
        if (result == EXIT_SUCCESS) {
            downloaded_artifacts.insert(artifact_url);
            artifact_downloads++;
            append_lines(state_downloaded, {artifact_url});
        } else {
            artifact_failures++;
            append_lines(state_failed, {artifact_url});
        }
    }

    spdlog::info(
        "VVII crawl abgeschlossen: seiten={}, seiten-fehler={}, "
        "artefakte-entdeckt={}, artefakte-heruntergeladen={}, "
        "artefakte-fehler={}",
        page_urls.size(),
        page_download_failures,
        all_discovered_artifacts.size(),
        artifact_downloads,
        artifact_failures);

    return artifact_failures > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
