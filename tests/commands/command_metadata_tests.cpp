// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "app/config.h"
#include "commands/cmd.dataset-report.h"
#include "commands/cmd.download-files.h"
#include "commands/cmd.download-toc.h"
#include "commands/cmd.format-xml.h"
#include "commands/cmd.help.h"
#include "commands/cmd.unzip-files.h"
#include "commands/cmd.version.h"

TEST_CASE("command metadata is stable", "[commands][metadata]")
{
    AppConfig config;

    HelpCommand const help(config);
    VersionCommand const version(config);
    DownloadTocCommand const download_toc(config);
    DownloadFilesCommand const download_files(config);
    UnzipFilesCommand const unzip(config);
    FormatXmlCommand const format_xml(config);
    DatasetReportCommand const report(config);

    REQUIRE(help.name() == "HelpCommand");
    REQUIRE(help.description() == "Zeige die Hilfe für jeden Befehl an.");
    REQUIRE(help.args_help_text() == std::vector<std::string_view>{"Zeige diese Hilfe an."});
    REQUIRE(help.group() == CommandGroup::General);
    REQUIRE(help.args() == std::vector<std::string_view>{"-h", "--help"});

    REQUIRE(version.name() == "VersionCommand");
    REQUIRE(version.description() == "Zeige Versionsinformationen in verschiedenen Formaten an.");
    REQUIRE(version.args_help_text().size() == 5);
    REQUIRE(version.group() == CommandGroup::General);
    REQUIRE(version.args().size() == 10);

    REQUIRE(download_toc.name() == "DownloadTocCommand");
    REQUIRE(download_toc.description() == "Download der TOC-Datei für die aktive Quelle.");
    REQUIRE(
        download_toc.args_help_text() == std::vector<std::string_view>{"Das Inhaltsverzeichnis (XML) herunterladen."});
    REQUIRE(download_toc.group() == CommandGroup::Functional);
    REQUIRE(download_toc.args() == std::vector<std::string_view>{"-t", "--toc"});

    REQUIRE(download_files.name() == "DownloadFilesCommand");
    REQUIRE(download_files.description() == "Alle Dateien gemäß aktiver Quelle herunterladen.");
    REQUIRE(
        download_files.args_help_text() == std::vector<std::string_view>{"Alle Dateien gemäß Quelle herunterladen "
                                                                         "(gii: ZIP/XML, vvii: HTML + Artefakte)."});
    REQUIRE(download_files.group() == CommandGroup::Functional);
    REQUIRE(download_files.args() == std::vector<std::string_view>{"-d", "--download"});

    REQUIRE(unzip.name() == "UnzipFilesCommand");
    REQUIRE(unzip.description() == "Entpackt alle Dateien in einzelne Ordner.");
    REQUIRE(unzip.args_help_text() == std::vector<std::string_view>{"Archivdateien entpacken."});
    REQUIRE(unzip.group() == CommandGroup::Functional);
    REQUIRE(unzip.args() == std::vector<std::string_view>{"-u", "--unzip"});

    REQUIRE(format_xml.name() == "FormatXmlCommand");
    REQUIRE(format_xml.description() == "XML-Dateien formatiert ausgeben.");
    REQUIRE(format_xml.args_help_text() == std::vector<std::string_view>{"XML-Dateien formatiert ausgeben."});

    REQUIRE(format_xml.group() == CommandGroup::Functional);
    REQUIRE(format_xml.args() == std::vector<std::string_view>{"-f", "--format"});

    REQUIRE(report.name() == "DatasetReportCommand");
    REQUIRE(report.description() == "Datensatz-Bericht nach Validierung erstellen.");
    REQUIRE(report.args_help_text() == std::vector<std::string_view>{"Datensatz-Bericht nach Validierung erstellen."});
    REQUIRE(report.group() == CommandGroup::Functional);
    REQUIRE(report.args() == std::vector<std::string_view>{"-r", "--report"});
}
