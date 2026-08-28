// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

// argc, argv
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

#include <spdlog/spdlog.h>

// std::format unter MSVC verwenden - libfmt unter Linux als Polyfill verwenden
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

#include "version.h"

#include "commands/cmd.base.h"

// alle Befehle einbinden
#include "commands/cmd.bgbl.h"
#include "commands/cmd.dataset-report.h"
#include "commands/cmd.download-files.h"
#include "commands/cmd.download-toc.h"
#include "commands/cmd.format-xml.h"
#include "commands/cmd.help.h"
#include "commands/cmd.unzip-files.h"
#include "commands/cmd.version.h"

int gdl_main(int argc, char** argv);
