// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "utils/download.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

int download_file(std::string const & url, std::string const & filename, bool const curlVerbose)
{
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        spdlog::error("curl konnte nicht initialisiert werden.");
        return EXIT_FAILURE;
    }

    std::unique_ptr<FILE, FileCloser> fp;
#if defined(_MSC_VER) && (defined(_WIN32) || defined(_WIN64))
    FILE* raw_fp = nullptr;
    fopen_s(&raw_fp, filename.c_str(), "wb");
    fp.reset(raw_fp);
#else
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    fp.reset(fopen(filename.c_str(), "wb"));
#endif
    if (fp == nullptr) {
        spdlog::error("{} konnte nicht zum Schreiben geöffnet werden.", filename);
        curl_easy_cleanup(curl);
        return EXIT_FAILURE;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    struct curl_slist* resolve_list = nullptr;
    resolve_list                    = curl_slist_append(resolve_list, "gesetze-im-internet.de:443:195.74.94.216");
    resolve_list                    = curl_slist_append(resolve_list, "gesetze-im-internet.de:80:195.74.94.216");
    resolve_list = curl_slist_append(resolve_list, "www.verwaltungsvorschriften-im-internet.de:443:195.74.94.215");
    resolve_list = curl_slist_append(resolve_list, "www.verwaltungsvorschriften-im-internet.de:80:195.74.94.215");
    if (spdlog::should_log(spdlog::level::info)) {
        spdlog::info(
            "Nutze feste DNS-Resolve-Einträge für gesetze-im-internet.de "
            "und verwaltungsvorschriften-im-internet.de.");
    }
    curl_easy_setopt(curl, CURLOPT_RESOLVE, resolve_list);
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

    std::string const useragent = format("Ontolaw-GDL/{}", app_version::get_version());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, useragent.c_str());

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp.get());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, curlVerbose ? 1L : 0L);

    // force HTTP 1.1 with SSL connection using TLSv1.3
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3);

    // SSL support
    curl_easy_setopt(curl, CURLOPT_CAINFO, "ca-bundle.crt");
    // current working dir
    curl_easy_setopt(curl, CURLOPT_CAPATH, ".");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    // WARNING
    // The server certificate of the host "gesetze-im-internet.de" is for
    // "juris.de". This means the host verification fails with a name mismatch. A
    // proper SSL handshake is not possible. ¯\_(ツ)_/¯

    // 2L
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode const res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        spdlog::error("Download von {} fehlgeschlagen: {}.", url, curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        fp.reset();
#if defined(_MSC_VER) && (defined(_WIN32) || defined(_WIN64))
        _unlink(filename.c_str());
#else
        ::unlink(filename.c_str());
#endif
        return EXIT_FAILURE;
    }

    curl_easy_cleanup(curl);

    curl_slist_free_all(resolve_list);

    fp.reset();

    return EXIT_SUCCESS;
}

bool try_get_file_and_folder(
    std::string const & url,
    std::filesystem::path const & outputRoot,
    std::filesystem::path& filepath,
    std::filesystem::path& folder)
{
    // Example URL: http://www.gesetze-im-internet.de/1-dm-goldm_nzg/xml.zip
    // We assume a static prefix and static suffix on all URLs.

    constexpr std::string_view http_prefix  = "http://www.gesetze-im-internet.de/";
    constexpr std::string_view https_prefix = "https://www.gesetze-im-internet.de/";
    // Remove suffix length of  8 (/xml.zip)
    constexpr int suffix_length = 8;

    std::size_t prefix_length = 0;
    if (url.starts_with(http_prefix)) {
        prefix_length = http_prefix.size();
    } else if (url.starts_with(https_prefix)) {
        prefix_length = https_prefix.size();
    } else {
        return false;
    }

    if (!url.ends_with("/xml.zip")) {
        return false;
    }

    if (url.size() <= prefix_length + static_cast<std::size_t>(suffix_length)) {
        return false;
    }

    // => 1-dm-goldm_nzg/xml.zip
    std::string const filePath = url.substr(prefix_length);
    // => 1-dm-goldm_nzg
    std::string const directory = filePath.substr(0, filePath.size() - suffix_length);

    folder   = outputRoot / std::filesystem::path(directory.c_str());
    filepath = folder / "xml.zip";

    folder.make_preferred();
    filepath.make_preferred();

    return true;
}

DownloadOutputMode select_download_output_mode(bool const curlVerbose, bool const ciEnvironment, bool const stderrIsTty)
{
    if (curlVerbose || ciEnvironment || !stderrIsTty) {
        return DownloadOutputMode::Compact;
    }
    return DownloadOutputMode::Interactive;
}

struct private_data
{
    int num_url = 0;
    std::string filename;
};

struct ActiveTransferInfo
{
    std::string filename;
    curl_off_t downloaded_bytes   = 0;
    curl_off_t total_bytes        = 0;
    double speed_bytes_per_second = 0.0;
};

struct DownloadProgressState
{
    int total                                                 = 0;
    int finished                                              = 0;
    int succeeded                                             = 0;
    int failed                                                = 0;
    int skipped                                               = 0;
    DownloadOutputMode output_mode                            = DownloadOutputMode::Interactive;
    std::chrono::steady_clock::time_point started_at          = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point last_compact_log_at = started_at;
    int last_compact_log_finished                             = 0;
    bool interactive_rendered                                 = false;
};

bool env_flag_enabled(char const * name)
{
    std::string value;
#ifdef _MSC_VER
    char* env           = nullptr;
    std::size_t env_len = 0;
    if (_dupenv_s(&env, &env_len, name) != 0 || env == nullptr) {
        return false;
    }
    value = env;
    std::free(env);
#else
    char const * env = std::getenv(name);
    if (env == nullptr) {
        return false;
    }
    value = env;
#endif
    if (value.empty()) {
        return false;
    }

    std::ranges::transform(value, value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    return value != "0" && value != "false";
}

bool is_ci_environment()
{ return env_flag_enabled("CI") || env_flag_enabled("GITHUB_ACTIONS"); }

bool stderr_is_tty()
{ return ISATTY(FILENO(stderr)) != 0; }

DownloadOutputMode determine_output_mode(bool const curlVerbose)
{ return select_download_output_mode(curlVerbose, is_ci_environment(), stderr_is_tty()); }

std::string shorten_filename(std::string const & filename, std::size_t const max_len = 60)
{
    if (filename.size() <= max_len) {
        return filename;
    }

    std::size_t const keep = (max_len - 3) / 2;
    std::size_t const tail = max_len - 3 - keep;
    return filename.substr(0, keep) + "..." + filename.substr(filename.size() - tail);
}

std::string join_filenames(std::vector<std::string> const & files, std::size_t const max_items)
{
    if (files.empty()) {
        return "-";
    }

    std::ostringstream output;
    std::size_t const shown = std::min(max_items, files.size());
    for (std::size_t i = 0; i < shown; ++i) {
        if (i > 0) {
            output << " | ";
        }
        output << shorten_filename(files.at(i));
    }
    if (files.size() > shown) {
        output << " | +" << (files.size() - shown) << " more";
    }

    return output.str();
}

std::string format_duration(std::chrono::seconds const duration)
{
    auto total_seconds = duration.count();
    auto const hours   = total_seconds / 3600;
    total_seconds %= 3600;
    auto const minutes = total_seconds / 60;
    auto const seconds = total_seconds % 60;

    std::ostringstream output;
    if (hours > 0) {
        output << hours << "h";
    }
    if (minutes > 0 || hours > 0) {
        output << minutes << "m";
    }
    output << seconds << "s";
    return output.str();
}

std::string format_clock_duration(std::chrono::seconds const duration)
{
    auto const total_seconds = std::max<int64_t>(0, duration.count());
    auto const hours         = total_seconds / 3600;
    auto const minutes       = (total_seconds % 3600) / 60;
    auto const seconds       = total_seconds % 60;

    std::ostringstream output;
    output << std::setfill('0');
    if (hours > 0) {
        output << std::setw(2) << hours << ":";
    }
    output << std::setw(2) << minutes << ":" << std::setw(2) << seconds;
    return output.str();
}

std::pair<double, std::string> normalize_bytes(double const bytes)
{
    constexpr std::array<char const *, 5> units = {"B", "KB", "MB", "GB", "TB"};
    double value                                = std::max(0.0, bytes);
    std::size_t unit_index                      = 0;
    while (value >= 1024.0 && unit_index + 1 < units.size()) {
        value /= 1024.0;
        ++unit_index;
    }

    return {value, units.at(unit_index)};
}

std::string format_bytes_value(double const bytes, int const decimals = 1)
{
    auto const [value, unit] = normalize_bytes(bytes);
    std::ostringstream output;
    output << std::fixed << std::setprecision(decimals) << value << " " << unit;
    return output.str();
}

std::string format_bytes_progress(curl_off_t const downloaded, curl_off_t const total)
{
    double const downloaded_f = static_cast<double>(std::max<curl_off_t>(0, downloaded));
    double const total_f      = static_cast<double>(std::max<curl_off_t>(0, total));

    if (total_f <= 0.0) {
        return format_bytes_value(downloaded_f);
    }

    constexpr std::array<char const *, 5> units = {"B", "KB", "MB", "GB", "TB"};
    std::size_t unit_index                      = 0;
    double scale                                = 1.0;
    double const max_bytes                      = std::max(downloaded_f, total_f);
    while ((max_bytes / scale) >= 1024.0 && unit_index + 1 < units.size()) {
        scale *= 1024.0;
        ++unit_index;
    }

    std::ostringstream output;
    output << std::fixed << std::setprecision(1) << (downloaded_f / scale) << "/" << (total_f / scale) << " "
           << units.at(unit_index);
    return output.str();
}

std::string render_progress_bar(double const ratio, int const bar_width = 32)
{
    double const clamped = std::clamp(ratio, 0.0, 1.0);
    int const filled     = static_cast<int>(std::round(clamped * static_cast<double>(bar_width)));

    std::string bar;
    bar.reserve(static_cast<std::size_t>(bar_width) * 3);
    for (int i = 0; i < bar_width; ++i) {
        bar += (i < filled) ? "█" : "░";
    }
    return bar;
}

std::string basename_of(std::string const & filepath)
{
    if (filepath.empty()) {
        return "-";
    }
    return std::filesystem::path(filepath).filename().string();
}

double estimate_progress_ratio(
    DownloadProgressState const & state, std::vector<ActiveTransferInfo> const & active_transfers)
{
    if (state.total <= 0) {
        return 1.0;
    }

    double partial = 0.0;
    for (auto const & transfer : active_transfers) {
        if (transfer.total_bytes <= 0) {
            continue;
        }
        double const downloaded = static_cast<double>(std::max<curl_off_t>(0, transfer.downloaded_bytes));
        double const total      = static_cast<double>(transfer.total_bytes);
        partial += std::clamp(downloaded / total, 0.0, 1.0);
    }

    double const done = static_cast<double>(std::max(0, state.finished));
    return std::clamp((done + partial) / static_cast<double>(state.total), 0.0, 1.0);
}

ActiveTransferInfo const * select_current_transfer(std::vector<ActiveTransferInfo> const & active_transfers)
{
    if (active_transfers.empty()) {
        return nullptr;
    }

    auto it =
        std::ranges::max_element(active_transfers, [](ActiveTransferInfo const & lhs, ActiveTransferInfo const & rhs) {
            if (lhs.downloaded_bytes == rhs.downloaded_bytes) {
                return lhs.speed_bytes_per_second < rhs.speed_bytes_per_second;
            }
            return lhs.downloaded_bytes < rhs.downloaded_bytes;
        });
    return &(*it);
}

std::string maybe_colorize_symbol(std::string const & symbol, bool const success)
{
    char const * color = success ? "\033[32m" : "\033[31m";
    return std::string(color) + symbol + "\033[0m";
}

std::vector<ActiveTransferInfo> collect_active_transfers(
    std::vector<CURL*> const & curl_easy_handles, std::vector<bool> const & transfer_done)
{
    std::vector<ActiveTransferInfo> active_transfers;

    for (std::size_t i = 0; i < curl_easy_handles.size(); ++i) {
        CURL* handle = curl_easy_handles.at(i);
        if (handle == nullptr || transfer_done.at(i)) {
            continue;
        }

        curl_socket_t active_socket     = CURL_SOCKET_BAD;
        CURLcode const socket_info_code = curl_easy_getinfo(handle, CURLINFO_ACTIVESOCKET, &active_socket);
        if (socket_info_code != CURLE_OK || active_socket == CURL_SOCKET_BAD) {
            continue;
        }

        private_data const * data = nullptr;
        curl_easy_getinfo(handle, CURLINFO_PRIVATE, &data);

        curl_off_t downloaded = 0;
        curl_off_t total      = 0;
        double speed          = 0.0;

        curl_easy_getinfo(handle, CURLINFO_SIZE_DOWNLOAD_T, &downloaded);
        curl_easy_getinfo(handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &total);
        curl_easy_getinfo(handle, CURLINFO_SPEED_DOWNLOAD_T, &speed);

        ActiveTransferInfo transfer;
        transfer.filename               = (data != nullptr) ? data->filename : std::string{};
        transfer.downloaded_bytes       = std::max<curl_off_t>(0, downloaded);
        transfer.total_bytes            = std::max<curl_off_t>(0, total);
        transfer.speed_bytes_per_second = std::max(0.0, speed);

        active_transfers.push_back(std::move(transfer));
    }

    return active_transfers;
}

std::vector<std::string> collect_active_files(
    std::vector<CURL*> const & curl_easy_handles, std::vector<bool> const & transfer_done)
{
    std::vector<std::string> active_files;
    auto const active_transfers = collect_active_transfers(curl_easy_handles, transfer_done);
    active_files.reserve(active_transfers.size());
    for (auto const & transfer : active_transfers) {
        if (!transfer.filename.empty()) {
            active_files.push_back(transfer.filename);
        }
    }

    return active_files;
}

void render_interactive_status(
    DownloadProgressState const & state, std::vector<ActiveTransferInfo> const & active_transfers)
{
    auto const now       = std::chrono::steady_clock::now();
    auto const elapsed   = std::chrono::duration_cast<std::chrono::seconds>(now - state.started_at);
    auto const * current = select_current_transfer(active_transfers);

    std::string const current_file =
        (current != nullptr) ? shorten_filename(basename_of(current->filename), 52) : std::string{"-"};

    std::string const byte_progress = (current != nullptr) ?
                                          format_bytes_progress(current->downloaded_bytes, current->total_bytes) :
                                          std::string{"-"};

    std::string const throughput = (current != nullptr && current->speed_bytes_per_second > 0.0) ?
                                       (format_bytes_value(current->speed_bytes_per_second, 1) + "/s") :
                                       std::string{"-"};

    std::chrono::seconds eta{0};
    bool eta_known = false;
    if (current != nullptr && current->speed_bytes_per_second > 0.0 && current->total_bytes > 0) {
        double const remaining =
            static_cast<double>(std::max<curl_off_t>(0, current->total_bytes - current->downloaded_bytes));
        eta       = std::chrono::seconds(static_cast<int64_t>(remaining / current->speed_bytes_per_second));
        eta_known = true;
    }

    std::string const line1 = format(
        "{} ▸ {} ▸ {} ▸ verbleibend: {}",
        current_file,
        byte_progress,
        throughput,
        eta_known ? format_clock_duration(eta) : std::string{"--:--"});

    double const progress_ratio = estimate_progress_ratio(state, active_transfers);
    int const percent           = static_cast<int>(std::round(progress_ratio * 100.0));
    std::string const bar       = render_progress_bar(progress_ratio);

    std::string const ok_symbol   = maybe_colorize_symbol("✓", true);
    std::string const fail_symbol = maybe_colorize_symbol("✗", false);

    std::string const line2 = format(
        "[{}] {}% ▸ {}/{} Dateien ▸ {}{} {}{} ⏱ {}",
        bar,
        percent,
        state.finished,
        state.total,
        ok_symbol + " ",
        state.succeeded,
        fail_symbol + " ",
        state.failed,
        format_clock_duration(elapsed));

    if (!state.interactive_rendered) {
        std::cerr << line1 << "\n" << line2 << std::flush;
        return;
    }

    std::cerr << "\033[2A\r\033[2K" << line1 << "\n\r\033[2K" << line2 << std::flush;
}

void render_interactive_status(DownloadProgressState const & state, std::vector<std::string> const & active_files)
{
    std::vector<ActiveTransferInfo> active_transfers;
    active_transfers.reserve(active_files.size());
    for (auto const & filename : active_files) {
        ActiveTransferInfo transfer;
        transfer.filename = filename;
        active_transfers.push_back(std::move(transfer));
    }

    render_interactive_status(state, active_transfers);
}

void maybe_render_compact_status(
    DownloadProgressState& state, std::vector<ActiveTransferInfo> const & active_transfers, bool const force = false)
{
    auto const now                    = std::chrono::steady_clock::now();
    int const finished_delta          = state.finished - state.last_compact_log_finished;
    bool const reached_step           = finished_delta >= 100;
    bool const reached_time           = (now - state.last_compact_log_at) >= std::chrono::seconds(30);
    bool const all_done               = state.total > 0 && state.finished >= state.total;
    bool const already_logged_current = (state.last_compact_log_finished == state.finished);

    if (all_done && already_logged_current) {
        return;
    }

    if (!force && !reached_step && !reached_time && !all_done) {
        return;
    }

    double const progress_ratio = estimate_progress_ratio(state, active_transfers);
    int const percent           = static_cast<int>(std::round(progress_ratio * 100.0));

    auto const elapsed   = std::chrono::duration_cast<std::chrono::seconds>(now - state.started_at);
    auto const * current = select_current_transfer(active_transfers);

    std::chrono::seconds eta{0};
    bool eta_known = false;
    if (current != nullptr && current->speed_bytes_per_second > 0.0 && current->total_bytes > 0) {
        double const remaining =
            static_cast<double>(std::max<curl_off_t>(0, current->total_bytes - current->downloaded_bytes));
        eta       = std::chrono::seconds(static_cast<int64_t>(remaining / current->speed_bytes_per_second));
        eta_known = true;
    }

    std::string const current_file =
        (current != nullptr) ? shorten_filename(basename_of(current->filename), 40) : std::string{"-"};

    std::string const byte_progress = (current != nullptr) ?
                                          format_bytes_progress(current->downloaded_bytes, current->total_bytes) :
                                          std::string{"-"};

    std::string const throughput = (current != nullptr && current->speed_bytes_per_second > 0.0) ?
                                       (format_bytes_value(current->speed_bytes_per_second, 1) + "/s") :
                                       std::string{"-"};

    std::cerr << "[download] " << percent << "%"
              << " dateien=" << state.finished << "/" << state.total << " ✓ " << state.succeeded << " ✗ "
              << state.failed << " aktuell=" << current_file << " bytes=" << byte_progress << " speed=" << throughput
              << " eta=" << (eta_known ? format_clock_duration(eta) : std::string{"--:--"})
              << " gesamt=" << format_clock_duration(elapsed) << "\n";

    state.last_compact_log_at       = now;
    state.last_compact_log_finished = state.finished;
}

void maybe_render_compact_status(
    DownloadProgressState& state, std::vector<std::string> const & active_files, bool const force = false)
{
    std::vector<ActiveTransferInfo> active_transfers;
    active_transfers.reserve(active_files.size());
    for (auto const & filename : active_files) {
        ActiveTransferInfo transfer;
        transfer.filename = filename;
        active_transfers.push_back(std::move(transfer));
    }

    maybe_render_compact_status(state, active_transfers, force);
}

void render_download_status(
    DownloadProgressState& state, std::vector<ActiveTransferInfo> const & active_transfers, bool const force = false)
{
    if (state.output_mode == DownloadOutputMode::Interactive) {
        render_interactive_status(state, active_transfers);
        state.interactive_rendered = true;
        return;
    }

    maybe_render_compact_status(state, active_transfers, force);
}

void render_download_status(
    DownloadProgressState& state, std::vector<std::string> const & active_files, bool const force = false)
{
    std::vector<ActiveTransferInfo> active_transfers;
    active_transfers.reserve(active_files.size());
    for (auto const & filename : active_files) {
        ActiveTransferInfo transfer;
        transfer.filename = filename;
        active_transfers.push_back(std::move(transfer));
    }

    render_download_status(state, active_transfers, force);
}

std::vector<CURL*> setup_curl_handles(
    std::vector<std::string>& urls,
    std::vector<CURL*>& curl_handles,
    std::vector<std::unique_ptr<FILE, FileCloser>>& files,
    std::vector<bool>& scheduled,
    int& setup_failures,
    bool const curlVerbose,
    std::filesystem::path const & outputRoot)
{
    int const transfers_total = static_cast<int>(urls.size());

    // initialize curl easy handles for each URL
    for (int i = 0; i < transfers_total; i++) {
        curl_handles.at(i) = curl_easy_init();
        files.at(i).reset();
        scheduled.at(i) = false;

        if (curl_handles.at(i) == nullptr) {
            spdlog::error("CURL-Handle für URL {} konnte nicht initialisiert werden", urls.at(i));
            setup_failures++;
            continue;
        }

        // == Get file and folder from url

        // = http://www.gesetze-im-internet.de/1-dm-goldm_nzg/xml.zip
        std::string url = urls.at(i);
        std::filesystem::path filepath;
        std::filesystem::path folder;
        if (!try_get_file_and_folder(url, outputRoot, filepath, folder)) {
            spdlog::error(
                "Datei- und Ordnerpfad konnten aus URL {} nicht ermittelt "
                "werden. Download wird übersprungen.",
                url);
            curl_easy_cleanup(curl_handles.at(i));
            curl_handles.at(i) = nullptr;
            setup_failures++;
            continue;
        }

        // Create folder
        if (!std::filesystem::is_directory(folder)) {
            bool const created = std::filesystem::create_directories(folder);
            if (!created) {
                spdlog::error(
                    "Verzeichnis {} konnte nicht erstellt werden. Download "
                    "wird übersprungen.",
                    folder.string());
                curl_easy_cleanup(curl_handles.at(i));
                curl_handles.at(i) = nullptr;
                setup_failures++;
                continue;
            }
        }

        // Open file
#if defined(_MSC_VER) && (defined(_WIN32) || defined(_WIN64))
        FILE* raw_file = nullptr;
        fopen_s(&raw_file, filepath.string().c_str(), "wb");
        files.at(i).reset(raw_file);
#else
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        files.at(i).reset(fopen(filepath.string().c_str(), "wb"));
#endif
        if (files.at(i) == nullptr) {
            spdlog::error("{} konnte nicht zum Schreiben geöffnet werden.", filepath.string());
            curl_easy_cleanup(curl_handles.at(i));
            curl_handles.at(i) = nullptr;
            setup_failures++;
            continue;
        }
        spdlog::debug("File {}, Folder {}", filepath.string(), folder.string());
        std::filesystem::path const current_path = std::filesystem::current_path();
        spdlog::debug("Current path is: {}", current_path.string());

        // == Prepare curl_handle

        // set URL to download
        spdlog::info("Lade herunter: {}", url.c_str());
        curl_easy_setopt(curl_handles.at(i), CURLOPT_URL, url.c_str());

        /*struct curl_slist *resolve_list = NULL;
        resolve_list = curl_slist_append(resolve_list,
        "gesetze-im-internet.de:80:195.74.94.216"); resolve_list =
        curl_slist_append(resolve_list, "gesetze-im-internet.de:443:195.74.94.216");
        curl_easy_setopt(curl_handles[i], CURLOPT_RESOLVE, resolve_list);*/
        curl_easy_setopt(curl_handles.at(i), CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

        curl_easy_setopt(curl_handles.at(i), CURLOPT_WRITEDATA, files.at(i).get());
        curl_easy_setopt(curl_handles.at(i), CURLOPT_WRITEFUNCTION, fwrite);

        auto* data = new private_data(i, filepath.string());
        curl_easy_setopt(curl_handles.at(i), CURLOPT_PRIVATE, data);

        std::string const useragent = format("Ontolaw-GDL/{}", app_version::get_version());
        curl_easy_setopt(curl_handles.at(i), CURLOPT_USERAGENT, useragent.c_str());

        curl_easy_setopt(curl_handles.at(i), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl_handles.at(i), CURLOPT_MAXREDIRS, 5L);
        // HTTP Status Code >= 400
        curl_easy_setopt(curl_handles.at(i), CURLOPT_FAILONERROR, 1L);
        // skip all signal handling
        curl_easy_setopt(curl_handles.at(i), CURLOPT_NOSIGNAL, 1L);
        // no progress bar
        curl_easy_setopt(curl_handles.at(i), CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(curl_handles.at(i), CURLOPT_VERBOSE, curlVerbose ? 1L : 0L);

        // enable TCP keep-alive for this transfer
        // with an keep-alive idle time of 120 seconds
        // and an interval time between keep-alive probes of 60 seconds
        curl_easy_setopt(curl_handles.at(i), CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl_handles.at(i), CURLOPT_TCP_KEEPIDLE, 120L);
        curl_easy_setopt(curl_handles.at(i), CURLOPT_TCP_KEEPINTVL, 60L);

        // enable all built-in compressions with buffer size 512kB
        curl_easy_setopt(curl_handles.at(i), CURLOPT_ACCEPT_ENCODING, "");
        curl_easy_setopt(curl_handles.at(i), CURLOPT_BUFFERSIZE, 524288L);

        // max. download speed
        int64_t const max_speed_bytes_per_second = 1024L * 1024;
        curl_easy_setopt(curl_handles.at(i), CURLOPT_MAX_RECV_SPEED_LARGE, max_speed_bytes_per_second);

        // abort download if 15 seconds below speed limit of 1024 bytes
        // curl_easy_setopt(curl_handles[i], CURLOPT_LOW_SPEED_TIME, 15L);
        // curl_easy_setopt(curl_handles[i], CURLOPT_LOW_SPEED_LIMIT, 1024L);

        // force HTTP 1.1 with SSL connection using TLSv1.3
        curl_easy_setopt(curl_handles.at(i), CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
        // curl_easy_setopt(curl_handles[i], CURLOPT_SSLVERSION,
        // CURL_SSLVERSION_TLSv1_3);

        // Note:
        // a) all URLs start with HTTP
        // b) we disable SSL, because its broken
        curl_easy_setopt(curl_handles.at(i), CURLOPT_SSL_VERIFYPEER, 0L);
        // curl_easy_setopt(curl_handles[i], CURLOPT_SSL_VERIFYHOST, 0L);

        // SSL support
        /*if(url.starts_with("https")) {
            #if defined(_WIN32) && defined(CURLSSLOPT_NATIVE_CA)
            curl_easy_setopt(handle, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
            #endif
            curl_easy_setopt(curl_handles[i], CURLOPT_CAINFO, "ca-bundle.crt");
            curl_easy_setopt(curl_handles[i], CURLOPT_CAPATH, "."); // current
        working dir curl_easy_setopt(curl_handles[i], CURLOPT_SSL_VERIFYPEER, 0L);
            // WARNING
            // The server certificate of the host "gesetze-im-internet.de" is for
        "juris.de".
            // This means the host verification fails with a name mismatch.
            // A proper SSL handshake is not possible. ¯\_(ツ)_/¯
            curl_easy_setopt(curl_handles[i], CURLOPT_SSL_VERIFYHOST, 0L); // 2L
        }*/

        scheduled.at(i) = true;
    }

    return curl_handles;
}

CURLM* setup_curl_multi_handle(std::vector<CURL*> const & curl_easy_handles)
{
    // initialize curl multi handle
    CURLM* curlm_handle = curl_multi_init();

    // max number of connections to a single host
    curl_multi_setopt(curlm_handle, CURLMOPT_MAX_HOST_CONNECTIONS, 5L);

    // set a more conservative pipe length, default 5
    // the total number of requests in-flight is CURLMOPT_MAX_HOST_CONNECTIONS *
    // CURLMOPT_MAX_PIPELINE_LENGTH.
    curl_multi_setopt(curlm_handle, CURLMOPT_MAX_PIPELINE_LENGTH, 5L);

    // max simultaneously open connections across all hosts
    curl_multi_setopt(curlm_handle, CURLMOPT_MAX_TOTAL_CONNECTIONS, 2L);

    // size of connection cache, only keep 10 connections in cache
    curl_multi_setopt(curlm_handle, CURLMOPT_MAXCONNECTS, 10L);

    // add curl_easy_handles to curl_multi_handle
    for (auto* curl_easy_handle : curl_easy_handles) {
        if (curl_easy_handle == nullptr) {
            continue;
        }
        curl_multi_add_handle(curlm_handle, curl_easy_handle);
    }

    return curlm_handle;
}

int download_urls_with_curl(
    std::vector<std::string>& urls,
    DownloadProgressState& progress,
    bool const curlVerbose,
    std::filesystem::path const & outputRoot)
{
    int transfers_total    = static_cast<int>(urls.size());
    int transfers_finished = 0;

    std::vector<CURL*> curl_easy_handles(transfers_total);
    std::vector<std::unique_ptr<FILE, FileCloser>> files(transfers_total);
    std::vector<bool> transfer_done(transfers_total);
    int setup_failures = 0;

    // set up curl easy handles for each URL
    curl_easy_handles =
        setup_curl_handles(urls, curl_easy_handles, files, transfer_done, setup_failures, curlVerbose, outputRoot);

    render_download_status(progress, collect_active_transfers(curl_easy_handles, transfer_done));

    if (setup_failures > 0) {
        transfers_finished += setup_failures;
        progress.finished += setup_failures;
        progress.failed += setup_failures;
        render_download_status(progress, collect_active_transfers(curl_easy_handles, transfer_done));
    }

    // set up curl multi handle with the curl easy handles
    auto* curlm_handle = setup_curl_multi_handle(curl_easy_handles);

    // main loop
    int running_handles = 0;
    CURLMcode mcode     = CURLM_OK;
    for (;;) {
        int numfds = 0;
        if (mcode == CURLM_OK) {
            // wait for activity on the file descriptors
            mcode = curl_multi_poll(curlm_handle, nullptr, 0, 1000, &numfds);
            if (mcode != CURLM_OK) {
                spdlog::error("curl_multi_poll() fehlgeschlagen: {}", curl_multi_strerror(mcode));
                break;
            }
            // if there is no activity, sleep for a short time to avoid busy looping
            if (numfds == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        // perform pending transfers
        mcode = curl_multi_perform(curlm_handle, &running_handles);
        if (mcode != CURLM_OK) {
            spdlog::error("curl_multi_perform() fehlgeschlagen: {}", curl_multi_strerror(mcode));
            break;
        }

        // check the response status code for each completed transfer
        CURLMsg const * msg = nullptr;
        int msgs_left       = 0;
        msg                 = curl_multi_info_read(curlm_handle, &msgs_left);
        while ((msg != nullptr) && msg->msg == CURLMSG_DONE) {
            CURL* easy_handle = msg->easy_handle;

            private_data const * data = nullptr;
            curl_easy_getinfo(easy_handle, CURLINFO_PRIVATE, &data);

            int const url_index = (data != nullptr) ? data->num_url : -1;

            if (url_index >= 0 && url_index < transfers_total) {
                transfer_done.at(url_index) = true;
            }

            // NOLINTNEXTLINE(runtime/int) - curl API requires long
            long http_status_code = 0;
            curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &http_status_code);
            bool const transfer_success = (msg->data.result == CURLE_OK && http_status_code == 200);
            if (!transfer_success) {
                spdlog::error(
                    "Transfer fehlgeschlagen (curl={}, http={}) für {}",
                    curl_easy_strerror(msg->data.result),
                    http_status_code,
                    (data != nullptr) ? data->filename : std::string{"<unknown>"});

                if (url_index >= 0 && url_index < transfers_total && files.at(url_index) != nullptr) {
                    files.at(url_index).reset();
                }
                // Delete the file
                if (data != nullptr) {
                    std::filesystem::remove(data->filename);
                }

                progress.failed++;
            }
            if (transfer_success) {
                curl_off_t bytes_download = 0;
                curl_easy_getinfo(easy_handle, CURLINFO_SIZE_DOWNLOAD_T, &bytes_download);
                spdlog::info("Heruntergeladene Bytes: {}", bytes_download);

                if (url_index >= 0 && url_index < transfers_total && files.at(url_index) != nullptr) {
                    files.at(url_index).reset();
                }

                progress.succeeded++;
            }

            if (data != nullptr) {
                // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
                delete data;
                curl_easy_setopt(easy_handle, CURLOPT_PRIVATE, nullptr);
            }

            transfers_finished++;
            progress.finished++;

            render_download_status(progress, collect_active_transfers(curl_easy_handles, transfer_done));

            // Read the next message
            msg = curl_multi_info_read(curlm_handle, &msgs_left);
        }

        bool const all_downloads_finished = (transfers_total == transfers_finished);

        if (all_downloads_finished) {
            break;
        }
    }

    spdlog::info("Downloads abgeschlossen [{} von {}].", running_handles, transfers_finished, transfers_total);

    // cleanup
    for (auto* curl_easy_handle : curl_easy_handles) {
        if (curl_easy_handle == nullptr) {
            continue;
        }

        private_data const * data = nullptr;
        curl_easy_getinfo(curl_easy_handle, CURLINFO_PRIVATE, &data);
        if (data != nullptr) {
            // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
            delete data;
            curl_easy_setopt(curl_easy_handle, CURLOPT_PRIVATE, nullptr);
        }

        curl_multi_remove_handle(curlm_handle, curl_easy_handle);
        curl_easy_cleanup(curl_easy_handle);
    }

    for (auto& fp : files) {
        fp.reset();
    }

    // cleanup the multi stack
    curl_multi_cleanup(curlm_handle);

    return EXIT_SUCCESS;
}

int download_urls(
    std::vector<std::string> const & urls, bool const curlVerbose, std::filesystem::path const & outputRoot)
{
    int const urls_total = static_cast<int>(urls.size());
    // download files in chunks of x
    int const chunk_size = 9;

    DownloadProgressState progress;
    progress.total               = urls_total;
    progress.output_mode         = determine_output_mode(curlVerbose);
    progress.started_at          = std::chrono::steady_clock::now();
    progress.last_compact_log_at = progress.started_at;

    if (urls_total == 0) {
        render_download_status(progress, std::vector<ActiveTransferInfo>{}, true);
        return EXIT_SUCCESS;
    }

    std::vector<std::string> urls_to_download;
    urls_to_download.reserve(urls_total);
    for (auto const & url : urls) {
        std::filesystem::path filepath;
        std::filesystem::path folder;
        if (try_get_file_and_folder(url, outputRoot, filepath, folder) && std::filesystem::exists(filepath)) {
            spdlog::info("Bereits vorhanden, uebersprungen: {}", filepath.string());
            progress.skipped++;
            progress.finished++;
        } else {
            urls_to_download.push_back(url);
        }
    }

    if (progress.skipped > 0) {
        spdlog::info("{} Datei(en) bereits vorhanden, uebersprungen.", progress.skipped);
    }

    int const to_download_total = static_cast<int>(urls_to_download.size());
    for (int start_index = 0; start_index < to_download_total; start_index += chunk_size) {
        int end_index = start_index + chunk_size;
        spdlog::info("Lade URLs von Index {} bis {} herunter", start_index, end_index);

        std::vector<std::string> chunk_urls;
        for (int j = start_index; j < end_index && j < to_download_total; j++) {
            chunk_urls.push_back(urls_to_download.at(j));
        }

        download_urls_with_curl(chunk_urls, progress, curlVerbose, outputRoot);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    render_download_status(progress, std::vector<ActiveTransferInfo>{}, true);

    if (progress.output_mode == DownloadOutputMode::Interactive && progress.interactive_rendered) {
        std::cerr << "\n";
    }

    spdlog::info(
        "Download-Zusammenfassung: abgeschlossen {} von {}, erfolgreich "
        "{}, fehlgeschlagen {}, uebersprungen {}.",
        progress.finished,
        progress.total,
        progress.succeeded,
        progress.failed,
        progress.skipped);

    return EXIT_SUCCESS;
}
