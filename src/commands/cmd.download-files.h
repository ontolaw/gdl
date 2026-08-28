// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <spdlog/spdlog.h>
#include <filesystem>
#include <vector>

#include "commands/cmd.base.h"
#include "utils/crawler.h"
#include "utils/download.h"
#include "utils/xml.h"

class DownloadFilesCommand : public CliCommand
{
public:
    explicit DownloadFilesCommand(AppConfig& appConfig);

    std::string_view name() const override
    { return "DownloadFilesCommand"; }
    std::string_view description() const override
    { return "Alle Dateien gemäß aktiver Quelle herunterladen."; }
    std::vector<std::string_view> args() const override
    { return std::vector<std::string_view>{"-d", "--download"}; }
    std::vector<std::string_view> args_help_text() const override
    {
        return {
            "Alle Dateien gemäß Quelle herunterladen (gii: ZIP/XML, vvii: HTML "
            "+ Artefakte)."};
    }
    CommandGroup group() const override
    { return CommandGroup::Functional; }
    void run(std::string_view const & command) const override;
};
