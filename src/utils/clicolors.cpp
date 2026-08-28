// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "utils/clicolors.h"

#include <iostream>
#include <string>
#include <vector>

void print_status(std::string const & status_message, int indent_spaces, Color color)
{
    std::string const escape_code = "\033[0;" + std::to_string(static_cast<int>(color)) + "m";
    std::string const reset_code  = "\033[0m";
    std::string const indentation(indent_spaces, ' ');
    std::cout << indentation << escape_code << status_message << reset_code << '\n';
}

std::string format_status(std::string const & status_message, int indent_spaces, Color color)
{
    std::string const escape_code = "\033[0;" + std::to_string(static_cast<int>(color)) + "m";
    std::string const reset_code  = "\033[0m";
    std::string const indentation(indent_spaces, ' ');
    // + "\n"
    return indentation + escape_code + status_message + reset_code;
}

std::string format_options(std::vector<std::string_view> const & args)
{
    if (args.empty()) {
        return "";
    }
    std::ostringstream oss;
    oss << args.at(0);
    for (std::size_t i = 1; i < args.size(); ++i) {
        oss << ", ";
        if (i % 2 == 0) {
            oss << "\n";
        }
        oss << args.at(i);
    }
    return oss.str();
}

#ifdef _WIN32
std::string get_datetime()
{
    auto now  = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &time);
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}
#else
std::string get_datetime()
{
    auto now  = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}
#endif
