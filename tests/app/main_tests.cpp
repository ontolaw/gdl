// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include <array>
#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "app/main.h"

namespace
{
    struct CurrentPathGuard
    {
        explicit CurrentPathGuard(std::filesystem::path const & next) : previous(std::filesystem::current_path())
        { std::filesystem::current_path(next); }

        ~CurrentPathGuard()
        { std::filesystem::current_path(previous); }

        CurrentPathGuard(CurrentPathGuard const &)            = delete;
        CurrentPathGuard& operator=(CurrentPathGuard const &) = delete;
        CurrentPathGuard(CurrentPathGuard&&)                  = delete;
        CurrentPathGuard& operator=(CurrentPathGuard&&)       = delete;

        std::filesystem::path previous;
    };
} // namespace

TEST_CASE("gdl_main shows help when no command is passed", "[app][main]")
{
    std::array<char, 4> arg0{"gdl"};
    std::array<char*, 1> argv{arg0.data()};

    REQUIRE(gdl_main(static_cast<int>(argv.size()), argv.data()) == EXIT_SUCCESS);
}

TEST_CASE("gdl_main returns failure for unknown command", "[app][main]")
{
    std::array<char, 4> arg0{"gdl"};
    std::array<char, 17> arg1{"--does-not-exist"};
    std::array<char*, 2> argv{arg0.data(), arg1.data()};

    REQUIRE(gdl_main(static_cast<int>(argv.size()), argv.data()) == EXIT_FAILURE);
}

TEST_CASE("gdl_main handles version command", "[app][main]")
{
    std::array<char, 4> arg0{"gdl"};
    std::array<char, 10> arg1{"--version"};
    std::array<char*, 2> argv{arg0.data(), arg1.data()};

    REQUIRE(gdl_main(static_cast<int>(argv.size()), argv.data()) == EXIT_SUCCESS);
}

TEST_CASE("gdl_main accepts hidden download limit arg as global option", "[app][main]")
{
    std::array<char, 4> arg0{"gdl"};
    std::array<char, 10> arg1{"--version"};
    std::array<char, 19> arg2{"--download-limit=2"};
    std::array<char*, 3> argv{arg0.data(), arg1.data(), arg2.data()};

    REQUIRE(gdl_main(static_cast<int>(argv.size()), argv.data()) == EXIT_SUCCESS);
}

TEST_CASE("gdl_main accepts source arg as global option", "[app][main]")
{
    std::array<char, 4> arg0{"gdl"};
    std::array<char, 10> arg1{"--version"};
    std::array<char, 14> arg2{"--source=vvii"};
    std::array<char*, 3> argv{arg0.data(), arg1.data(), arg2.data()};

    REQUIRE(gdl_main(static_cast<int>(argv.size()), argv.data()) == EXIT_SUCCESS);
}

TEST_CASE("gdl_main handles report command in isolated workspace", "[app][main]")
{
    auto const temp_root = std::filesystem::temp_directory_path() / "gdl_main_report";
    auto const data_root = temp_root / "data";
    auto const folder    = data_root / AppConfig::RAW_EXTRACTED_DIRECTORY / "a";

    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(folder);
    {
        std::ofstream xml((folder / "one.xml").string());
        xml << "<root/>";
    }

    CurrentPathGuard const guard(temp_root);

    std::array<char, 4> arg0{"gdl"};
    std::array<char, 9> arg1{"--report"};
    std::array<char, 11> arg2{"--out=data"};
    std::array<char*, 3> argv{arg0.data(), arg1.data(), arg2.data()};

    REQUIRE(gdl_main(static_cast<int>(argv.size()), argv.data()) == EXIT_SUCCESS);
    REQUIRE(std::filesystem::exists(temp_root / "data" / "datensatz-bericht.md"));

    std::filesystem::remove_all(temp_root);
}
