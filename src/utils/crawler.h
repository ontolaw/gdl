// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <filesystem>
#include <optional>

int crawl_vvii_from_toc(
    std::filesystem::path const & tocFilepath,
    std::filesystem::path const & outputRoot,
    bool curlVerbose,
    std::optional<int> pageLimit = std::nullopt);
