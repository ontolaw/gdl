// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include <array>

#include <catch2/catch_test_macros.hpp>

#include "app/config.h"

TEST_CASE("AppConfig defaults are stable", "[config]")
{
    AppConfig config;

    REQUIRE(config.output_directory() == AppConfig::DEFAULT_OUTPUT_DIRECTORY_GII);
    REQUIRE(config.source() == AppConfig::DownloadSource::Gii);
    REQUIRE(config.source_name() == "gii");
    REQUIRE(config.toc_file() == AppConfig::TOC_FILE_GII);
    REQUIRE(config.toc_url() == AppConfig::TOC_URL_GII);
    REQUIRE(config.verbosity_level() == AppConfig::VerbosityLevel::Quiet);
    REQUIRE_FALSE(config.is_verbose());
    REQUIRE_FALSE(config.is_very_verbose());
}

TEST_CASE("AppConfig recognizes global args", "[config]")
{
    AppConfig const config;

    REQUIRE(config.is_global_arg("-v"));
    REQUIRE(config.is_global_arg("--verbose"));
    REQUIRE(config.is_global_arg("-vv"));
    REQUIRE(config.is_global_arg("--verbose-very"));
    REQUIRE(config.is_global_arg("--out=data/out"));
    REQUIRE(config.is_global_arg("-out=data/out"));
    REQUIRE(config.is_global_arg("-o=data/out"));
    REQUIRE(config.is_global_arg("--download-limit=3"));
    REQUIRE(config.is_global_arg("--source=vvii"));

    REQUIRE_FALSE(config.is_global_arg("--toc"));
    REQUIRE_FALSE(config.is_global_arg("-x"));
}

TEST_CASE("AppConfig updates verbosity and output directory from args", "[config]")
{
    AppConfig config;

    std::array<char, 4> arg0{"gdl"};
    std::array<char, 3> arg1{"-v"};
    std::array<char, 15> arg2{"--out=/tmp/gii"};
    std::array<char*, 3> argv1{arg0.data(), arg1.data(), arg2.data()};

    config.updateFromCommandLineArgs(static_cast<int>(argv1.size()), argv1.data());

    REQUIRE(config.is_verbose());
    REQUIRE_FALSE(config.is_very_verbose());
    REQUIRE(config.output_directory() == "tmp/gii");

    std::array<char, 4> arg3{"-vv"};
    std::array<char*, 2> argv2{arg0.data(), arg3.data()};
    config.updateFromCommandLineArgs(static_cast<int>(argv2.size()), argv2.data());

    REQUIRE(config.is_very_verbose());
}

TEST_CASE("AppConfig parses hidden download limit arg", "[config]")
{
    AppConfig config;

    std::array<char, 4> arg0{"gdl"};
    std::array<char, 19> arg1{"--download-limit=4"};
    std::array<char*, 2> argv{arg0.data(), arg1.data()};

    config.updateFromCommandLineArgs(static_cast<int>(argv.size()), argv.data());

    auto const download_limit = config.download_limit();
    REQUIRE(config.has_download_limit());
    REQUIRE(download_limit.has_value());
    REQUIRE(download_limit == 4);
}

TEST_CASE("AppConfig updates source and source-dependent defaults", "[config]")
{
    AppConfig config;

    std::array<char, 4> arg0{"gdl"};
    std::array<char, 14> arg1{"--source=vvii"};
    std::array<char*, 2> argv{arg0.data(), arg1.data()};

    config.updateFromCommandLineArgs(static_cast<int>(argv.size()), argv.data());

    REQUIRE(config.source() == AppConfig::DownloadSource::Vvii);
    REQUIRE(config.source_name() == "vvii");
    REQUIRE(config.output_directory() == AppConfig::DEFAULT_OUTPUT_DIRECTORY_VVII);
    REQUIRE(config.toc_file() == AppConfig::TOC_FILE_VVII);
    REQUIRE(config.toc_url() == AppConfig::TOC_URL_VVII);
}

TEST_CASE("AppConfig keeps custom output directory when source changes", "[config]")
{
    AppConfig config;

    std::array<char, 4> arg0{"gdl"};
    std::array<char, 16> arg1{"--out=my/output"};
    std::array<char, 14> arg2{"--source=vvii"};
    std::array<char*, 3> argv{arg0.data(), arg1.data(), arg2.data()};

    config.updateFromCommandLineArgs(static_cast<int>(argv.size()), argv.data());

    REQUIRE(config.source() == AppConfig::DownloadSource::Vvii);
    REQUIRE(config.output_directory() == "my/output");
}

TEST_CASE("AppConfig keeps invalid hidden download limit as empty", "[config]")
{
    AppConfig config;

    std::array<char, 4> arg0{"gdl"};
    std::array<char, 21> arg1{"--download-limit=abc"};
    std::array<char*, 2> argv{arg0.data(), arg1.data()};

    config.updateFromCommandLineArgs(static_cast<int>(argv.size()), argv.data());

    REQUIRE_FALSE(config.has_download_limit());
    REQUIRE_FALSE(config.download_limit().has_value());
}
