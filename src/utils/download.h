// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// use std::format on MSVC - use libfmt as polyfill on Linux
#ifdef _WIN32
#include <format>
using std::format;
using std::make_format_args;
using std::vformat;
#else
// #include <fmt/core.h>
#include <fmt/format.h>
using fmt::format;
using fmt::make_format_args;
using fmt::vformat;
#endif

#include <curl/curl.h>
#include <spdlog/spdlog.h>

#include "version.h"

struct FileCloser
{
    void operator()(FILE* fp) const
    {
        if (fp != nullptr) {
            std::fclose(fp);
        }
    }
};

enum class DownloadOutputMode
{
    Interactive,
    Compact
};

DownloadOutputMode select_download_output_mode(bool curlVerbose, bool ciEnvironment, bool stderrIsTty);
bool try_get_file_and_folder(
    std::string const & url,
    std::filesystem::path const & outputRoot,
    std::filesystem::path& filepath,
    std::filesystem::path& folder);

int download_file(std::string const & url, std::string const & filename, bool curlVerbose = false);
int download_urls(
    std::vector<std::string> const & urls,
    bool curlVerbose                         = false,
    std::filesystem::path const & outputRoot = "data/gii");
