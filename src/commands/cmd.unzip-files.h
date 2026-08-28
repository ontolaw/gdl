// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <spdlog/spdlog.h>

#include <vector>

#include "commands/cmd.base.h"
#include "utils/unzip.h"

class UnzipFilesCommand : public CliCommand
{
public:
    // Konstruktor
    explicit UnzipFilesCommand(AppConfig& appConfig);

    std::string_view name() const override
    { return "UnzipFilesCommand"; }
    std::string_view description() const override
    { return "Entpackt alle Dateien in einzelne Ordner."; }
    std::vector<std::string_view> args() const override
    { return {"-u", "--unzip"}; }
    std::vector<std::string_view> args_help_text() const override
    { return {"Archivdateien entpacken."}; }
    CommandGroup group() const override
    { return CommandGroup::Functional; }
    void run(std::string_view const & command) const override;
};
