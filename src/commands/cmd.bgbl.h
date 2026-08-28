// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <spdlog/spdlog.h>
#include <filesystem>
#include <vector>

#include "app/config.h"
#include "commands/cmd.base.h"
#include "utils/bgbl.h"

class BgblCommand : public CliCommand
{
public:
    explicit BgblCommand(AppConfig& appConfig);

    std::string_view name() const override
    { return "BgblCommand"; }
    std::string_view description() const override
    { return "Download von Bundesgesetzblättern (bgbl.de XAVER, OffeneGesetze.de oder Bund-RIS)."; }
    std::vector<std::string_view> args() const override
    { return {"download-toc", "download-issue", "download-year", "download-range"}; }
    std::vector<std::string_view> args_help_text() const override
    {
        return {
            "Verfügbare Ausgaben eines Jahrgangs auflisten (--year=, --part=).",
            "Eine einzelne Ausgabe herunterladen (--year=, --nr=, --part=).",
            "Alle Ausgaben eines Jahrgangs herunterladen (--year=, --part=).",
            "Einen Jahrgänge-Bereich herunterladen (--from=, --to=, --part=)."};
    }
    CommandGroup group() const override
    { return CommandGroup::Functional; }
    void run(std::string_view const & command) const override;
};
