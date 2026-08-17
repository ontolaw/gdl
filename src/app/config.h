// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include "utils/files.h"

class AppConfig
{
public:
    enum class DownloadSource
    {
        Gii,
        Vvii
    };

    enum class VerbosityLevel
    {
        Quiet       = 0,
        Verbose     = 1,
        VeryVerbose = 2
    };

    // Standardeinstellungen
    inline static bool const DEFAULT_DEBUG_MODE                   = 0;
    inline static std::string const DEFAULT_OUTPUT_DIRECTORY      = "data/gii";
    inline static std::string const DEFAULT_OUTPUT_DIRECTORY_GII  = "data/gii";
    inline static std::string const DEFAULT_OUTPUT_DIRECTORY_VVII = "data/vvii";
    inline static std::string const RAW_ZIPS_DIRECTORY            = "raw-zips";
    inline static std::string const RAW_EXTRACTED_DIRECTORY       = "raw-extracted";
    inline static std::string const TOC_URL_GII                   = "https://www.gesetze-im-internet.de/gii-toc.xml";
    inline static std::string const TOC_URL_VVII  = "https://www.verwaltungsvorschriften-im-internet.de/vvii-toc.xml";
    inline static std::string const TOC_FILE_GII  = "gii-toc.xml";
    inline static std::string const TOC_FILE_VVII = "vvii-toc.xml";
    inline static std::string const DATASET_REPORT_FILE = "datensatz-bericht.md";

    AppConfig() :
        debugMode_(DEFAULT_DEBUG_MODE),
        outputDirectory_(DEFAULT_OUTPUT_DIRECTORY),
        source_(DownloadSource::Gii),
        verbosityLevel_(VerbosityLevel::Quiet)
    {
        // empty
    }

    // Kommandozeilenargumente parsen und AppConfig aktualisieren
    void updateFromCommandLineArgs(int argc, char* argv[])
    {
        for (int i = 1; i < argc; i++) {
            std::string const arg = argv[i];
            apply_global_arg(arg);
        }
    }

    bool is_global_arg(std::string_view const arg) const
    {
        return is_output_directory_arg(arg) || is_verbose_arg(arg) || is_very_verbose_arg(arg) ||
               is_download_limit_arg(arg) || is_source_arg(arg);
    }

    VerbosityLevel verbosity_level() const
    { return verbosityLevel_; }

    bool is_verbose() const
    { return verbosityLevel_ >= VerbosityLevel::Verbose; }

    bool is_very_verbose() const
    { return verbosityLevel_ == VerbosityLevel::VeryVerbose; }

    std::string output_directory()
    { return outputDirectory_; }

    DownloadSource source() const
    { return source_; }

    std::string source_name() const
    { return source_ == DownloadSource::Vvii ? "vvii" : "gii"; }

    std::string toc_url() const
    { return source_ == DownloadSource::Vvii ? TOC_URL_VVII : TOC_URL_GII; }

    std::string toc_file() const
    { return source_ == DownloadSource::Vvii ? TOC_FILE_VVII : TOC_FILE_GII; }

    void set_output_directory(std::string const & str)
    {
        // Führenden Slash entfernen, um Probleme mit std::filesystem::path zu
        // vermeiden
        outputDirectory_       = remove_leading_slash(str);
        customOutputDirectory_ = true;
    }

    bool has_download_limit() const
    { return downloadLimit_.has_value(); }

    std::optional<int> download_limit() const
    { return downloadLimit_; }

private:
    bool apply_global_arg(std::string const & arg)
    {
        if (is_output_directory_arg(arg)) {
            set_output_directory(output_directory_from_arg(arg));
            return true;
        }

        if (is_very_verbose_arg(arg)) {
            verbosityLevel_ = VerbosityLevel::VeryVerbose;
            return true;
        }

        if (is_verbose_arg(arg) && verbosityLevel_ < VerbosityLevel::Verbose) {
            verbosityLevel_ = VerbosityLevel::Verbose;
            return true;
        }

        if (is_download_limit_arg(arg)) {
            downloadLimit_ = download_limit_from_arg(arg);
            return true;
        }

        if (is_source_arg(arg)) {
            auto source = source_from_arg(arg);
            if (source.has_value()) {
                source_ = source.value();
                if (!customOutputDirectory_) {
                    outputDirectory_ = default_output_directory_for_source(source_);
                }
            }
            return true;
        }

        return false;
    }

    static bool starts_with(std::string_view const value, std::string_view const prefix)
    { return value.rfind(prefix, 0) == 0; }

    static bool is_output_directory_arg(std::string_view const arg)
    { return starts_with(arg, "-out=") || starts_with(arg, "-o=") || starts_with(arg, "--out="); }

    static bool is_verbose_arg(std::string_view const arg)
    { return arg == "-v" || arg == "--verbose"; }

    static bool is_very_verbose_arg(std::string_view const arg)
    { return arg == "-vv" || arg == "--verbose-very"; }

    static bool is_download_limit_arg(std::string_view const arg)
    { return starts_with(arg, "--download-limit="); }

    static bool is_source_arg(std::string_view const arg)
    { return starts_with(arg, "--source="); }

    static std::string output_directory_from_arg(std::string const & arg)
    {
        if (starts_with(arg, "--out=")) {
            return arg.substr(6);
        }
        if (starts_with(arg, "-out=")) {
            return arg.substr(5);
        }
        if (starts_with(arg, "-o=")) {
            return arg.substr(3);
        }
        return arg;
    }

    static std::optional<int> download_limit_from_arg(std::string const & arg)
    {
        // strlen("--download-limit=")
        constexpr std::size_t prefix_length = 17;
        auto const value_part               = arg.substr(prefix_length);
        if (value_part.empty()) {
            return std::nullopt;
        }

        try {
            std::size_t position = 0;
            int const value      = std::stoi(value_part, &position);
            if (position != value_part.size()) {
                return std::nullopt;
            }
            return value;
        } catch (...) {
            return std::nullopt;
        }
    }

    static std::optional<DownloadSource> source_from_arg(std::string const & arg)
    {
        // strlen("--source=")
        constexpr std::size_t prefix_length = 9;
        if (arg.size() <= prefix_length) {
            return std::nullopt;
        }

        auto const value_part = arg.substr(prefix_length);
        if (value_part == "gii") {
            return DownloadSource::Gii;
        }
        if (value_part == "vvii") {
            return DownloadSource::Vvii;
        }

        return std::nullopt;
    }

    static std::string default_output_directory_for_source(DownloadSource const source)
    { return source == DownloadSource::Vvii ? DEFAULT_OUTPUT_DIRECTORY_VVII : DEFAULT_OUTPUT_DIRECTORY_GII; }

    // Überschreibbare/veränderbare Standardeinstellungen
    bool debugMode_;
    std::string outputDirectory_;
    DownloadSource source_;
    bool customOutputDirectory_ = false;
    VerbosityLevel verbosityLevel_;
    std::optional<int> downloadLimit_;
};
