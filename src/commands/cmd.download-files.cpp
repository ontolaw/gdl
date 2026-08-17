// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "commands/cmd.download-files.h"

#include <string>

DownloadFilesCommand::DownloadFilesCommand(AppConfig& appConfig) : CliCommand(appConfig)
{
    // empty
}

void DownloadFilesCommand::run(std::string_view const & command) const
{
    // not used
    (void)command;

    auto const limit = appConfig_.download_limit();
    if (limit.has_value() && (*limit < 1 || *limit > 10)) {
        spdlog::error(
            "Ungültiger Wert für versteckten Parameter "
            "--download-limit={}. Erlaubt sind nur Werte von 1 bis 10.",
            *limit);
        return;
    }

    spdlog::info("Lade Dateien herunter (Quelle: {})", appConfig_.source_name());

    // determine full path to the TOC file (output directory + filename)
    std::string const toc_file         = appConfig_.toc_file();
    std::string const output_folder    = appConfig_.output_directory();
    std::filesystem::path toc_filepath = output_folder;
    toc_filepath /= toc_file;
    toc_filepath.make_preferred();

    if (appConfig_.source() == AppConfig::DownloadSource::Vvii) {
        int const result = crawl_vvii_from_toc(toc_filepath, output_folder, appConfig_.is_very_verbose(), limit);
        if (result != EXIT_SUCCESS) {
            spdlog::error("VVII Crawl ist mit Fehlern beendet worden.");
        }
        return;
    }

    // parses the gii-toc.xml file and extract the URLs
    auto urls = get_urls(toc_filepath.string());

    if (limit.has_value() && urls.size() > static_cast<std::size_t>(*limit)) {
        urls.resize(static_cast<std::size_t>(*limit));
        spdlog::info(
            "Versteckter Testmodus aktiv: Es werden nur {} Datei(en) "
            "heruntergeladen.",
            *limit);
    }

    spdlog::info("Anzahl herunterzuladender Dateien: {}", urls.size());

    std::filesystem::path raw_zips_folder = appConfig_.output_directory();
    raw_zips_folder /= AppConfig::RAW_ZIPS_DIRECTORY;

    download_urls(urls, appConfig_.is_very_verbose(), raw_zips_folder);
}
