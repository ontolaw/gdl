// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "commands/cmd.version.h"

#include <spdlog/spdlog.h>

#include <iostream>
#include <print>

VersionCommand::VersionCommand(AppConfig& appConfig) : CliCommand(appConfig)
{
    // empty
}

void VersionCommand::run(std::string_view const & command) const
{
    if (command == "-V" || command == "--version") {
        VersionCommand::showVersion();
    } else if (command == "-Vo" || command == "--version-only") {
        VersionCommand::showVersionOnly();
    } else if (command == "-Vj" || command == "--version-json") {
        VersionCommand::showVersionJson();
    } else if (command == "-Vb" || command == "--version-build") {
        VersionCommand::showVersionBuild();
    } else if (command == "-Vbj" || command == "--version-build-json") {
        VersionCommand::showVersionBuildJson();
    } else {
        // Ungültiger Befehl
        spdlog::error("Unbekannter Versionsbefehl: {}", command);
        spdlog::error("Verwende -V oder --version für Hilfe.");
    }
}

void VersionCommand::showVersion()
{
    // -V, --version
    std::println("{} v{}", app_version::get_binary_name(), app_version::get_version());
}

void VersionCommand::showVersionOnly()
{
    // -vo, --version-only
    std::println("{}", app_version::get_version());
}

void VersionCommand::showVersionJson()
{
    // -vj, --version-json
    std::cout << app_version::get_version_json() << "\n";
}

void VersionCommand::showVersionBuild()
{
    // -Vb, --version-build
    std::cout << app_version::get_version_build() << "\n";
}

void VersionCommand::showVersionBuildJson()
{
    // -Vbj, --version-build-json
    std::cout << app_version::get_version_build_json() << "\n";
}
