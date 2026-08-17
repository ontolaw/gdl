// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <chrono>
#include <ctime>
#include <iomanip>

enum class Color
{
    Red        = 31,
    Yellow     = 33,
    Green      = 32,
    Blue       = 34,
    Purple     = 35,
    Cyan       = 36,
    Light_Grey = 37
};

void print_status(
    std::string const & status_message = "Status update.", int indent_spaces = 0, Color color = Color::Light_Grey);
std::string format_status(
    std::string const & status_message = "Status update.", int indent_spaces = 0, Color color = Color::Light_Grey);
std::string format_options(std::vector<std::string_view> const & args);
std::string get_datetime();
