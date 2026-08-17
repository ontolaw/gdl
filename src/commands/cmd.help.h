// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

// setw
#include <iomanip>
#include <iostream>
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

#include "utils/clicolors.h"

#include "commands/cmd.base.h"
#include "version.h"

#include "commands/cmd.download-files.h"
#include "commands/cmd.download-toc.h"
#include "commands/cmd.help.h"
#include "commands/cmd.unzip-files.h"
#include "commands/cmd.version.h"

class HelpCommand : public CliCommand
{
public:
    // Konstruktor
    explicit HelpCommand(AppConfig& appConfig);

    // Überschreibt Basisklassenfunktionen
    std::string_view name() const override
    { return "HelpCommand"; }
    std::string_view description() const override
    { return "Zeige die Hilfe für jeden Befehl an."; }
    std::vector<std::string_view> args() const override
    { return {"-h", "--help"}; }
    std::vector<std::string_view> args_help_text() const override
    { return {"Zeige diese Hilfe an."}; }
    CommandGroup group() const override
    { return CommandGroup::General; }
    void run(std::string_view const & command = "") const override;

    // Setter für die Befehlstabelle
    void setCommands(std::vector<CliCommand*> c)
    { commands_ = c; }

private:
    std::vector<CliCommand*> commands_;

    std::string color_format_args(std::string& arg1, std::string& arg2) const;

    // Formatbreite für die Hilfeausgabe
    static constexpr std::size_t HELP_FORMAT_WIDTH = 40;
};
