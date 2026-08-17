// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <vector>

#include "commands/cmd.base.h"
#include "version.h"

class VersionCommand : public CliCommand
{
public:
    // Konstruktor
    explicit VersionCommand(AppConfig& appConfig);

    // Überschreibt Basisklassenfunktionen
    std::string_view name() const override
    { return "VersionCommand"; }
    std::string_view description() const override
    { return "Zeige Versionsinformationen in verschiedenen Formaten an."; }
    std::vector<std::string_view> args() const override
    {
        return {
            "-V",
            "--version",
            "-Vo",
            "--version-only",
            "-Vj",
            "--version-json",
            "-Vb",
            "--version-build",
            "-Vbj",
            "--version-build-json"};
    }
    std::vector<std::string_view> args_help_text() const override
    {
        return {
            "Zeige Versionsinformationen an.",
            "Zeige nur die Versionsnummer an.",
            "Zeige Versionsinformationen im JSON-Format an.",
            "Zeige Build-Informationen an.",
            "Zeige Build-Informationen im JSON-Format an."};
    }
    CommandGroup group() const override
    { return CommandGroup::General; }
    void run(std::string_view const & command) const override;

private:
    static void showVersion();
    static void showVersionOnly();
    static void showVersionJson();
    static void showVersionBuild();
    static void showVersionBuildJson();
};
