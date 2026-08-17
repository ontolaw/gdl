// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <spdlog/spdlog.h>
#include <filesystem>
#include <vector>

#include "commands/cmd.base.h"
#include "utils/download.h"

class DownloadTocCommand : public CliCommand
{
public:
    explicit DownloadTocCommand(AppConfig& appConfig);

    std::string_view name() const override
    { return "DownloadTocCommand"; }
    std::string_view description() const override
    { return "Download der TOC-Datei für die aktive Quelle."; }
    std::vector<std::string_view> args() const override
    { return {"-t", "--toc"}; }
    std::vector<std::string_view> args_help_text() const override
    { return {"Das Inhaltsverzeichnis (XML) herunterladen."}; }
    CommandGroup group() const override
    { return CommandGroup::Functional; }
    void run(std::string_view const & command) const override;
};
