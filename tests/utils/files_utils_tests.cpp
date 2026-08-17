// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include <filesystem>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "utils/files.h"

TEST_CASE("remove_leading_slash handles common inputs", "[files]")
{
    REQUIRE(remove_leading_slash("/abc") == "abc");
    REQUIRE(remove_leading_slash("abc") == "abc");
    REQUIRE(remove_leading_slash("").empty());
    REQUIRE(remove_leading_slash("//abc") == "/abc");
}

TEST_CASE("remove_color_codes strips ansi escape sequences", "[files]")
{
    std::string const input = "\x1b[0;31mError\x1b[0m and \x1b[1;33mWarn\x1b[0m";
    REQUIRE(remove_color_codes(input) == "Error and Warn");
}

TEST_CASE("ensure_writeable grants owner write permission recursively", "[files]")
{
    auto const temp_root = std::filesystem::temp_directory_path() / "gdl_files_utils_test";
    auto const nested    = temp_root / "nested";
    auto const file_path = nested / "sample.txt";

    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(nested);
    {
        std::ofstream out(file_path.string());
        out << "x";
    }

    std::filesystem::permissions(
        temp_root,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace);
    std::filesystem::permissions(file_path, std::filesystem::perms::owner_read, std::filesystem::perm_options::replace);

    ensure_writeable(temp_root);

    auto const perms_after = std::filesystem::status(file_path).permissions();
    REQUIRE((perms_after & std::filesystem::perms::owner_write) != std::filesystem::perms::none);

    std::filesystem::permissions(temp_root, std::filesystem::perms::all, std::filesystem::perm_options::replace);
    std::filesystem::remove_all(temp_root);
}
