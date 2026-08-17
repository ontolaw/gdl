// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "commands/cmd.format-xml.h"

#include <string>

FormatXmlCommand::FormatXmlCommand(AppConfig& appConfig) : CliCommand(appConfig)
{
    // empty
}

void FormatXmlCommand::run(std::string_view const & command) const
{
    // not used
    (void)command;

    std::filesystem::path output_folder = appConfig_.output_directory();
    output_folder /= AppConfig::RAW_EXTRACTED_DIRECTORY;

    spdlog::info("Formatiere XML-Dateien in {}", output_folder.string());

    format_xml_files(output_folder.string());

    spdlog::info("Fertig");
}
