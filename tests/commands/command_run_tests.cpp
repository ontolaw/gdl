// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "app/config.h"
#include "commands/cmd.dataset-report.h"
#include "commands/cmd.download-files.h"
#include "commands/cmd.download-toc.h"
#include "commands/cmd.format-xml.h"
#include "commands/cmd.help.h"
#include "commands/cmd.unzip-files.h"
#include "commands/cmd.version.h"

namespace
{
    class CurrentPathGuard
    {
    public:
        explicit CurrentPathGuard(std::filesystem::path const & next) : previous_(std::filesystem::current_path())
        { std::filesystem::current_path(next); }

        ~CurrentPathGuard()
        { std::filesystem::current_path(previous_); }

        CurrentPathGuard(CurrentPathGuard const &)            = delete;
        CurrentPathGuard& operator=(CurrentPathGuard const &) = delete;
        CurrentPathGuard(CurrentPathGuard&&)                  = delete;
        CurrentPathGuard& operator=(CurrentPathGuard&&)       = delete;

    private:
        std::filesystem::path previous_;
    };
} // namespace

TEST_CASE("HelpCommand renders configured command list", "[commands][run]")
{
    AppConfig config;

    HelpCommand help(config);
    VersionCommand version(config);
    DownloadFilesCommand download(config);
    help.setCommands({&help, &version, &download});

    std::ostringstream const out;
    auto* prev = std::cout.rdbuf(out.rdbuf());
    help.run("--help");
    std::cout.rdbuf(prev);

    auto const rendered = out.str();
    REQUIRE(rendered.contains("Usage:"));
    REQUIRE(rendered.contains("--help"));
    REQUIRE(rendered.contains("--version"));
    REQUIRE(rendered.contains("--download"));
    REQUIRE(rendered.contains("--source=SRC"));
}

TEST_CASE("VersionCommand renders json and build output", "[commands][run]")
{
    AppConfig config;
    VersionCommand const version(config);

    std::ostringstream const out;
    auto* prev = std::cout.rdbuf(out.rdbuf());

    version.run("-Vj");
    version.run("-Vb");
    version.run("-Vbj");
    version.run("--version-json");

    std::cout.rdbuf(prev);

    auto const rendered = out.str();
    REQUIRE(rendered.contains("\"version\""));
    REQUIRE(rendered.contains("Build Information"));
    REQUIRE(rendered.contains("\"triplet\""));
}

TEST_CASE("VersionCommand handles version-only and unknown subcommand", "[commands][run]")
{
    AppConfig config;
    VersionCommand const version(config);

    version.run("-Vo");
    version.run("--unknown-version-subcommand");

    SUCCEED();
}

TEST_CASE("DownloadTocCommand skips existing toc file", "[commands][run]")
{
    AppConfig config;
    auto const temp_root = std::filesystem::current_path() / "tmp" / "gdl_cmd_download_toc";
    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(temp_root);

    config.set_output_directory("tmp/gdl_cmd_download_toc");

    auto const toc_file = temp_root / config.toc_file();
    {
        std::ofstream out(toc_file.string());
        out << "<items/>";
    }

    DownloadTocCommand const cmd(config);
    cmd.run("--toc");

    REQUIRE(std::filesystem::exists(toc_file));

    std::filesystem::remove_all(temp_root);
}

TEST_CASE("DownloadFilesCommand handles empty toc list", "[commands][run]")
{
    AppConfig config;
    auto const temp_root = std::filesystem::current_path() / "tmp" / "gdl_cmd_download_files";
    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(temp_root);

    config.set_output_directory("tmp/gdl_cmd_download_files");

    auto const toc_file = temp_root / config.toc_file();
    {
        std::ofstream out(toc_file.string());
        out << "<items></items>";
    }

    DownloadFilesCommand const cmd(config);
    cmd.run("--download");

    REQUIRE(std::filesystem::exists(toc_file));

    std::filesystem::remove_all(temp_root);
}

TEST_CASE("DownloadFilesCommand rejects hidden limit outside allowed range", "[commands][run]")
{
    AppConfig config;

    std::array<char, 4> arg0{"gdl"};
    std::array<char, 20> arg1{"--download-limit=11"};
    std::array<char*, 2> argv{arg0.data(), arg1.data()};
    config.updateFromCommandLineArgs(static_cast<int>(argv.size()), argv.data());

    DownloadFilesCommand const cmd(config);
    cmd.run("--download");

    SUCCEED();
}

TEST_CASE("FormatXmlCommand formats XML in configured output folder", "[commands][run]")
{
    AppConfig config;
    auto const temp_root = std::filesystem::current_path() / "tmp" / "gdl_cmd_format_xml";
    auto const nested    = temp_root / AppConfig::RAW_EXTRACTED_DIRECTORY / "nested";
    auto const xml_file  = nested / "sample.xml";

    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(nested);

    {
        std::ofstream out(xml_file.string());
        out << "<root><child>ok</child></root>";
    }

    config.set_output_directory("tmp/gdl_cmd_format_xml");

    FormatXmlCommand const cmd(config);
    cmd.run("--format");

    REQUIRE(std::filesystem::exists(xml_file));
    std::ifstream in(xml_file.string());
    std::string const content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    REQUIRE(content.contains("<child>ok</child>"));

    std::filesystem::remove_all(temp_root);
}

TEST_CASE("DatasetReportCommand writes report file", "[commands][run]")
{
    AppConfig config;
    auto const temp_root = std::filesystem::current_path() / "tmp" / "gdl_cmd_dataset_report";
    auto const data_root = temp_root / "data";
    auto const one       = data_root / AppConfig::RAW_EXTRACTED_DIRECTORY / "one";
    auto const two       = data_root / AppConfig::RAW_EXTRACTED_DIRECTORY / "two";

    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(one);
    std::filesystem::create_directories(two);

    {
        std::ofstream xml((one / "a.xml").string());
        xml << "<root/>";
    }
    {
        std::ofstream other((one / "readme.txt").string());
        other << "x";
    }
    {
        std::ofstream other((two / "note.txt").string());
        other << "y";
    }

    config.set_output_directory("data");

    DatasetReportCommand const cmd(config);
    {
        CurrentPathGuard const guard(temp_root);
        cmd.run("--report");
    }

    auto const report_file = temp_root / "data" / AppConfig::DATASET_REPORT_FILE;
    REQUIRE(std::filesystem::exists(report_file));

    std::ifstream in(report_file.string());
    std::string const content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    REQUIRE(content.contains("Datensatz-Report"));
    REQUIRE(content.contains("Ordner mit XML-Datei"));

    std::filesystem::remove_all(temp_root);
}

TEST_CASE("UnzipFilesCommand runs on empty data/gii folder", "[commands][run]")
{
    AppConfig config;
    UnzipFilesCommand const cmd(config);

    auto const temp_root = std::filesystem::current_path() / "tmp" / "gdl_cmd_unzip";
    auto const data_dir  = temp_root / "data" / "gii";

    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(data_dir);

    {
        CurrentPathGuard const guard(temp_root);
        cmd.run("--unzip");
    }

    REQUIRE(std::filesystem::exists(data_dir));

    std::filesystem::remove_all(temp_root);
}
