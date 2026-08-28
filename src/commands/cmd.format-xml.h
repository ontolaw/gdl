// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <spdlog/spdlog.h>

#include <vector>

#include "commands/cmd.base.h"
#include "utils/xml.h"

class FormatXmlCommand : public CliCommand
{
public:
    explicit FormatXmlCommand(AppConfig& appConfig);

    std::string_view name() const override
    { return "FormatXmlCommand"; }
    std::string_view description() const override
    { return "XML-Dateien formatiert ausgeben."; }
    std::vector<std::string_view> args() const override
    { return {"-f", "--format"}; }
    std::vector<std::string_view> args_help_text() const override
    { return {"XML-Dateien formatiert ausgeben."}; }
    CommandGroup group() const override
    { return CommandGroup::Functional; }
    void run(std::string_view const & command) const override;
};
