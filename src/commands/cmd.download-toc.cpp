// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "commands/cmd.download-toc.h"

#include <string>

DownloadTocCommand::DownloadTocCommand(AppConfig& appConfig) : CliCommand(appConfig)
{
    // empty
}

void DownloadTocCommand::run(std::string_view const & command) const
{
    // not used
    (void)command;

    spdlog::info("Lade TOC für Quelle '{}' herunter", appConfig_.source_name());

    std::string const toc_url  = appConfig_.toc_url();
    std::string const toc_file = appConfig_.toc_file();

    std::string const output_folder = appConfig_.output_directory();

    // filepath to toc_file is output_path + toc_file
    std::filesystem::path toc_filepath = output_folder;
    toc_filepath /= toc_file;
    // slash fix
    toc_filepath.make_preferred();

    if (std::filesystem::exists(toc_filepath)) {
        spdlog::info("Überspringe Download von {}.", toc_url);
        spdlog::info("Datei existiert bereits: {}.", toc_filepath.string());
    } else {
        // create output folders
        std::filesystem::create_directories(toc_filepath.parent_path());

        if (download_file(toc_url, toc_filepath.string(), appConfig_.is_very_verbose()) != 0) {
            spdlog::error("Download fehlgeschlagen: {}.", toc_url);
            exit(EXIT_FAILURE);
        } else {
            spdlog::info("Download erfolgreich.");
        }
    }
}
