// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "utils/download.h"

TEST_CASE("select_download_output_mode chooses compact or interactive", "[download][smoke]")
{
    REQUIRE(select_download_output_mode(false, false, true) == DownloadOutputMode::Interactive);
    REQUIRE(select_download_output_mode(true, false, true) == DownloadOutputMode::Compact);
    REQUIRE(select_download_output_mode(false, true, true) == DownloadOutputMode::Compact);
    REQUIRE(select_download_output_mode(false, false, false) == DownloadOutputMode::Compact);
}

TEST_CASE("try_get_file_and_folder parses expected gii URLs", "[download][smoke]")
{
    std::filesystem::path filepath;
    std::filesystem::path folder;
    auto const output_root = std::filesystem::path("data") / "gii";

    REQUIRE(try_get_file_and_folder(
        "http://www.gesetze-im-internet.de/1-dm-goldm_nzg/xml.zip", output_root, filepath, folder));
    REQUIRE(filepath == std::filesystem::path("data/gii/1-dm-goldm_nzg/xml.zip"));
    REQUIRE(folder == std::filesystem::path("data/gii/1-dm-goldm_nzg"));

    REQUIRE(try_get_file_and_folder(
        "https://www.gesetze-im-internet.de/abfallr_anl_2/xml.zip", output_root, filepath, folder));

    REQUIRE_FALSE(try_get_file_and_folder("http://example.invalid/a/xml.zip", output_root, filepath, folder));
    REQUIRE_FALSE(try_get_file_and_folder("bad-url", output_root, filepath, folder));
}

TEST_CASE("download_urls handles empty input", "[download][smoke]")
{
    std::vector<std::string> const urls;
    REQUIRE(download_urls(urls, false, "data/gii") == EXIT_SUCCESS);
}
