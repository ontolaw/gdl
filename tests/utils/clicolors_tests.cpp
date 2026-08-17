// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "utils/clicolors.h"

namespace
{
    class CoutBufferGuard
    {
    public:
        explicit CoutBufferGuard(std::streambuf* next) : previous_(std::cout.rdbuf(next)) { }

        CoutBufferGuard(CoutBufferGuard const &)            = delete;
        CoutBufferGuard& operator=(CoutBufferGuard const &) = delete;
        CoutBufferGuard(CoutBufferGuard&&)                  = delete;
        CoutBufferGuard& operator=(CoutBufferGuard&&)       = delete;

        ~CoutBufferGuard()
        { std::cout.rdbuf(previous_); }

    private:
        std::streambuf* previous_;
    };
} // namespace

TEST_CASE("format_status uses ANSI color and indentation", "[clicolors]")
{
    std::string const formatted = format_status("Hello", 2, Color::Green);

    REQUIRE(formatted.starts_with("  \x1b[0;32m"));
    REQUIRE(formatted.ends_with("\x1b[0m"));
    REQUIRE(formatted.contains("Hello"));
}

TEST_CASE("format_options returns comma separated list with line breaks", "[clicolors]")
{
    std::vector<std::string_view> const args{"-a", "--alpha", "-b", "--beta"};
    auto const out = format_options(args);

    REQUIRE(out == "-a, --alpha, \n-b, --beta");
    REQUIRE(format_options({}).empty());
}

TEST_CASE("get_datetime returns expected timestamp format", "[clicolors]")
{
    auto const stamp = get_datetime();
    std::regex const re("^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}$");
    REQUIRE(std::regex_match(stamp, re));
}

TEST_CASE("print_status writes formatted line to stdout", "[clicolors]")
{
    std::ostringstream const capture;
    {
        CoutBufferGuard const guard(capture.rdbuf());
        print_status("Hello", 1, Color::Red);
    }

    auto const out = capture.str();
    REQUIRE(out.contains("\x1b[0;31m"));
    REQUIRE(out.contains("Hello"));
    REQUIRE(out.ends_with("\n"));
}
