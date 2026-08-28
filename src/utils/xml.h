// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "utils/files.h"

#include <pugixml.hpp>

std::vector<std::string> get_urls(std::string const & toc_file);
void format_xml_files(std::string const & folder);
