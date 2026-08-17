// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

void ensure_writeable(std::filesystem::path const & folder);
std::string remove_leading_slash(std::string const & str);
std::string remove_color_codes(std::string const & input_text);
