// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "commands/cmd.help.h"

#include <iostream>
#include <string>

HelpCommand::HelpCommand(AppConfig& appConfig) : CliCommand(appConfig)
{
    // empty
}

std::string HelpCommand::color_format_args(std::string& arg1, std::string& arg2) const
{
    std::string const combined_args = fmt::format("{:<4} {}", arg1, arg2);
    std::string formatted_args      = format_status(combined_args, 2, Color::Green);
    return formatted_args;
}

void HelpCommand::run(std::string_view const & /*command*/) const
{
    // NOTE do not use std::format(), only format(), see header (polyfill).
    // TODO(JensA.Koch): how to use format() with argid's, e.g {nice-name} +
    // format_arg()?

    // Header
    std::string const help_text_header = fmt::format(
        "{} {}\n"
        "{}\n\n"
        "{}\n\n"
        "{} {} [OPTIONEN] [ARGUMENTE]\n\n",
        format_status(app_version::get_binary_name(), 0, Color::Yellow).c_str(),
        format_status(app_version::get_version(), 0, Color::Yellow).c_str(),
        app_version::get_copyright(),
        app_version::get_description(),
        format_status("Usage:", 0, Color::Yellow).c_str(),
        app_version::get_binary_name());

    // Gruppierte Ausgabe der Options vorbereiten
    std::string general_options;
    std::string functional_options;

    for (auto const & command_ : commands_) {
        auto const & group          = command_->group();
        auto const & args           = command_->args();
        auto const & args_help_text = command_->args_help_text();

        for (std::size_t i = 0; i < args.size(); i += 2) {
            // format args
            std::string arg1           = std::string(args.at(i));
            std::string arg2           = (i + 1 < args.size()) ? std::string(args.at(i + 1)) : "";
            std::string formatted_args = color_format_args(arg1, arg2);

            // format args_help_text
            std::string formatted_args_help_text;
            if (!args_help_text.empty() && i / 2 < args_help_text.size()) {
                formatted_args_help_text = args_help_text.at(i / 2);
            }

            std::string const option_line =
                fmt::format("{:<{}} {}\n", formatted_args, HELP_FORMAT_WIDTH, formatted_args_help_text);

            if (group == CommandGroup::General) {
                general_options += option_line;
            } else {
                functional_options += option_line;
            }
        }
    }

    // Ausgabe gruppierter Optionen
    std::string const help_text_options = fmt::format(
        "{}\n{}\n{}\n{}\n",
        format_status("Allgemeine Optionen:", 0, Color::Yellow),
        "Optionen, die das Verhalten des Tools steuern.\n\n" +
            (general_options.empty() ? "  (none)\n" : general_options),

        format_status("Funktionale Optionen:", 0, Color::Yellow),
        "Optionen, die spezifische Aufgaben ausführen und kombiniert werden "
        "können.\n\n" +
            (functional_options.empty() ? "  (none)\n" : functional_options));

    // Ausgabe zusätzlicher, spezieller Argumente
    std::string out_arg1{"-o=DIR"};
    std::string out_arg2{"--out=DIR"};
    std::string const formatted_out_args = color_format_args(out_arg1, out_arg2);

    std::string verbose_arg1{"-v"};
    std::string verbose_arg2{"--verbose"};
    std::string const formatted_verbose_args = color_format_args(verbose_arg1, verbose_arg2);

    std::string very_verbose_arg1{"-vv"};
    std::string very_verbose_arg2{"--verbose-very"};
    std::string const formatted_very_verbose_args = color_format_args(very_verbose_arg1, very_verbose_arg2);

    std::string source_arg1;
    std::string source_arg2{"--source=SRC"};
    std::string const formatted_source_args = color_format_args(source_arg1, source_arg2);

    std::string const help_text_arguments = fmt::format(
        "{}\n"
        "{:<{}} {}\n"
        "{:<{}} {}\n"
        "{:<{}} {}\n"
        "{:<{}} {}\n",
        format_status("Argumente:", 0, Color::Yellow).c_str(),
        formatted_verbose_args,
        HELP_FORMAT_WIDTH,
        "Ausführliche Protokollierung (Info/Fortschritt/Warnungen).",
        formatted_very_verbose_args,
        HELP_FORMAT_WIDTH,
        "Sehr ausführliche Protokollierung (inkl. cURL-Details).",
        formatted_source_args,
        HELP_FORMAT_WIDTH,
        "Downloadquelle wählen: gii oder vvii (Standard = gii).",
        formatted_out_args,
        HELP_FORMAT_WIDTH,
        std::string("Ausgabeverzeichnis festlegen (Standard: gii=") + AppConfig::DEFAULT_OUTPUT_DIRECTORY_GII +
            ", vvii=" + AppConfig::DEFAULT_OUTPUT_DIRECTORY_VVII + ").");

    std::cout << help_text_header << help_text_options << help_text_arguments;
}
