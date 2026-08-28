// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <filesystem>
#include <iostream>
#include <string>

#include "minizip/unzip.h"
#include "minizip/zip.h"

int extract_zips(std::string const & top_level_folder);
int extract_zips_to_folder(
    std::string const & source_root, std::string const & target_root, bool delete_source_zips = false);
