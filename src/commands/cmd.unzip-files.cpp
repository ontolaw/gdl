// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "commands/cmd.unzip-files.h"

#include <string>

UnzipFilesCommand::UnzipFilesCommand(AppConfig& appConfig) : CliCommand(appConfig)
{
    // empty
}

void UnzipFilesCommand::run(std::string_view const & command) const
{
    // not used
    (void)command;

    spdlog::info("Entpacke ZIP-Dateien...\n");

    std::filesystem::path const output_root = appConfig_.output_directory();
    std::filesystem::path const source_root = output_root / AppConfig::RAW_ZIPS_DIRECTORY;
    std::filesystem::path const target_root = output_root / AppConfig::RAW_EXTRACTED_DIRECTORY;

    auto extract_start = std::chrono::high_resolution_clock::now();

    extract_zips_to_folder(source_root.string(), target_root.string(), false);

    auto extract_end      = std::chrono::high_resolution_clock::now();
    auto extract_duration = std::chrono::duration_cast<std::chrono::milliseconds>(extract_end - extract_start);
    spdlog::info("Laufzeit: {} ms", extract_duration.count());
}
