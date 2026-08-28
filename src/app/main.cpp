// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "app/main.h"

#include <spdlog/sinks/stdout_color_sinks.h>

#include <algorithm>
#include <exception>
#include <iostream>
#include <span>
#include <string>
#include <vector>

int gdl_main(int argc, char** argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    AppConfig appConfig;
    std::span<char*> const raw_args(argv, static_cast<std::size_t>(argc));
    std::vector<std::string> const cli_args(raw_args.begin(), raw_args.end());
    appConfig.updateFromCommandLineArgs(cli_args);

    auto stderr_logger = spdlog::get("gdl");
    if (!stderr_logger) {
        stderr_logger = spdlog::stderr_color_mt("gdl");
    }
    spdlog::set_default_logger(stderr_logger);

    auto logLevel = spdlog::level::warn;
    if (appConfig.is_verbose()) {
#ifdef _DEBUG
        logLevel = spdlog::level::debug;
#else
        logLevel = spdlog::level::info;
#endif
    }
    spdlog::set_level(logLevel);

    // CLI-Befehle initialisieren
    HelpCommand helpCommand(appConfig);
    VersionCommand versionCommand(appConfig);
    BgblCommand bgblCommand(appConfig);
    DownloadTocCommand downloadTocCommand(appConfig);
    DownloadFilesCommand downloadFilesCommand(appConfig);
    FormatXmlCommand formatXmlCommandCommand(appConfig);
    UnzipFilesCommand unzipFilesCommand(appConfig);
    DatasetReportCommand datasetReportCommand(appConfig);

    std::vector<CliCommand*> const commands = {
        &helpCommand,
        &versionCommand,
        &bgblCommand,
        &downloadTocCommand,
        &downloadFilesCommand,
        &unzipFilesCommand,
        &formatXmlCommandCommand,
        &datasetReportCommand};

    helpCommand.setCommands(commands);

    // Mehrere Argumente sammeln
    std::span<char*> const args(argv, static_cast<std::size_t>(argc));
    std::vector<std::string_view> commandNames;
    bool sourcePrefixConsumed = false;
    bool bgblPrefixConsumed   = false;
    for (std::string_view const arg : args.subspan(1)) {
        if (appConfig.is_global_arg(arg)) {
            continue;
        }
        // Quell-Präfix (Subcommand) erkennen und verwerten: gdl gii … / gdl vvii …
        if (!sourcePrefixConsumed && AppConfig::is_source_prefix(arg)) {
            auto const source = AppConfig::source_from_prefix(arg);
            if (source) {
                appConfig.set_source(*source);
                sourcePrefixConsumed = true;
                spdlog::debug("Quelle über Subcommand-Präfix gewählt: {}", appConfig.source_name());
                continue;
            }
        }
        // Befehlsgruppen-Präfix erkennen und verwerten: gdl bgbl …
        if (!bgblPrefixConsumed && arg == "bgbl") {
            bgblPrefixConsumed = true;
            spdlog::debug("BGBl-Befehlsgruppe gewählt.");
            continue;
        }
        commandNames.emplace_back(arg);
    }

    // Kommandozeilenargumente auswerten und passende Befehle ausführen
    if (args.size() <= 1 || commandNames.empty()) {
        helpCommand.run();
        return EXIT_SUCCESS;
    }

    // Gespeicherte Befehle der Reihe nach ausführen
    for (auto const & commandName : commandNames) {
        CliCommand const * command = nullptr;
        spdlog::debug("Verfügbare Befehle:");
        spdlog::debug("{:<22} | {:^5} | {:<12}", "Befehl", "Arg 1", "Arg 2");
        for (auto const & cmd : commands) {
            auto const & commandArgs = cmd->args();
            spdlog::debug("{:<22} | {:^5} | {:<12}", cmd->name(), commandArgs.at(0), commandArgs.at(1));
            // Befehl zum angegebenen Befehlsnamen ermitteln
            bool const command_known = std::ranges::any_of(commandArgs, [&](std::string_view const arg) {
                return arg == commandName;
            });
            if (command_known) {
                command = cmd;
                break;
            }
        }

        if (command == nullptr) {
            spdlog::error("Unbekannter Befehl: {}", commandName);
            helpCommand.run();
            return EXIT_FAILURE;
        }

        command->run(commandName);
    }

    return EXIT_SUCCESS;
}

#ifndef GDL_DISABLE_MAIN
int main(int argc, char** argv)
{
    try {
        return gdl_main(argc, argv);
    } catch (std::exception const & e) {
        std::cerr << "Fehler: " << e.what() << '\n';
        return EXIT_FAILURE;
    } catch (...) {
        return EXIT_FAILURE;
    }
}
#endif
