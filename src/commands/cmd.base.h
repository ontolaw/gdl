// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "app/config.h"

enum class CommandGroup
{
    General,
    Functional
};

class CliCommand
{
public:
    explicit CliCommand(AppConfig& appConfig) : appConfig_(appConfig) { }
    virtual ~CliCommand()                                        = default;
    virtual std::string_view name() const                        = 0;
    virtual std::string_view description() const                 = 0;
    virtual std::vector<std::string_view> args() const           = 0;
    virtual std::vector<std::string_view> args_help_text() const = 0;
    virtual CommandGroup group() const                           = 0;
    virtual void run(std::string_view const & command) const     = 0;

protected:
    AppConfig& appConfig_;
};
