// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <string>
#include <vector>

// use std::format on MSVC - use libfmt as polyfill on Linux
#ifdef _WIN32
#include <format>
using std::format;
using std::make_format_args;
using std::vformat;
#else
// #include <fmt/core.h>
#include <fmt/format.h>
using fmt::format;
using fmt::make_format_args;
using fmt::vformat;
#endif

#include <spdlog/spdlog.h>

#include "utils/clicolors.h"
#include "utils/files.h"

#include "commands/cmd.base.h"

class DatasetReportCommand : public CliCommand
{
public:
    explicit DatasetReportCommand(AppConfig& appConfig);

    std::string_view name() const override
    { return "DatasetReportCommand"; }
    std::string_view description() const override
    { return "Datensatz-Bericht nach Validierung erstellen."; }
    std::vector<std::string_view> args() const override
    { return {"-r", "--report"}; }
    std::vector<std::string_view> args_help_text() const override
    { return {"Datensatz-Bericht nach Validierung erstellen."}; }
    CommandGroup group() const override
    { return CommandGroup::Functional; }
    void run(std::string_view const & command) const override;

    void generateFileReport(std::string const & outputRoot, std::string const & datasetRoot) const;
};
