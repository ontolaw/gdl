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
    inline static int const DEFAULT_BGBL_JOBS           = 4;

    AppConfig() :
        debugMode_(DEFAULT_DEBUG_MODE),
        outputDirectory_(DEFAULT_OUTPUT_DIRECTORY),
        source_(DownloadSource::Gii),
        verbosityLevel_(VerbosityLevel::Quiet)
    {
        // empty
    }

    // Kommandozeilenargumente parsen und AppConfig aktualisieren
    void updateFromCommandLineArgs(std::vector<std::string> const & args)
    {
        for (auto const & arg : args) {
            apply_global_arg(arg);
        }
    }

    bool is_global_arg(std::string_view const arg) const
    {
        return is_output_directory_arg(arg) || is_verbose_arg(arg) || is_very_verbose_arg(arg) ||
               is_download_limit_arg(arg) || is_source_arg(arg) || is_bgbl_opt_arg(arg);
    }

    // bgbl.de (Bundesgesetzblatt) Download-Optionen.
    bool is_bgbl_opt_arg(std::string_view const arg) const
    {
        return is_bgbl_year_arg(arg) || is_bgbl_nr_arg(arg) || is_bgbl_part_arg(arg) || is_bgbl_from_arg(arg) ||
               is_bgbl_to_arg(arg) || is_bgbl_jobs_arg(arg) || is_bgbl_source_arg(arg);
    }

    // Präfix-basierte Quellenwahl: `gdl gii …` / `gdl vvii …`
    static bool is_source_prefix(std::string_view const arg)
    { return arg == "gii" || arg == "vvii"; }

    static std::optional<DownloadSource> source_from_prefix(std::string_view const arg)
    {
        if (arg == "gii") {
            return DownloadSource::Gii;
        }
        if (arg == "vvii") {
            return DownloadSource::Vvii;
        }

        return std::nullopt;
    }

    VerbosityLevel verbosity_level() const
    { return verbosityLevel_; }

    bool is_verbose() const
    { return verbosityLevel_ >= VerbosityLevel::Verbose; }

    bool is_very_verbose() const
    { return verbosityLevel_ == VerbosityLevel::VeryVerbose; }

    std::string const & output_directory()
    { return outputDirectory_; }

    DownloadSource source() const
    { return source_; }

    std::string source_name() const
    { return source_ == DownloadSource::Vvii ? "vvii" : "gii"; }

    // Quelle explizit setzen (z. B. aus dem Subcommand-Präfix `gii`/`vvii`).
    // Setzt das Standard-Ausgabeverzeichnis zurück, sofern nicht überschrieben.
    void set_source(DownloadSource const source)
    {
        source_ = source;
        if (!customOutputDirectory_) {
            outputDirectory_ = default_output_directory_for_source(source);
        }
    }

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

    std::optional<int> bgbl_year() const
    { return bgblYear_; }

    std::optional<int> bgbl_nr() const
    {
        if (!bgblNrRaw_.has_value()) {
            return std::nullopt;
        }
        auto const & value = bgblNrRaw_.value();
        try {
            std::size_t pos = 0;
            int const n     = std::stoi(value, &pos);
            if (pos != value.size()) {
                return std::nullopt;
            }
            return n;
        } catch (...) {
            return std::nullopt;
        }
    }

    // Roher (nicht geparster) --nr=-Wert. BGBl-Ausgabenummern sind meist
    // numerisch, das Bund-RIS kann jedoch ergänzende Suffixe (z. B. "67-2")
    // verwenden; Aufrufer, die den exakten Wert (RIS-Quelle) benötigen,
    // verwenden dies anstelle von bgbl_nr().
    std::optional<std::string> bgbl_nr_raw() const
    { return bgblNrRaw_; }

    std::optional<int> bgbl_part() const
    { return bgblPart_; }

    std::optional<int> bgbl_from() const
    { return bgblFrom_; }

    std::optional<int> bgbl_to() const
    { return bgblTo_; }

    // BGBl-Download-Backend. „xaver" (Standard) verwendet das bgbl.de-XAVER-Backend,
    // „og" (oder „offenegesetze") verwendet die offene OffeneGesetze.de-REST-API.
    std::string bgbl_source() const
    { return bgblSource_.value_or("xaver"); }

    // Anzahl paralleler Downloads für `download-year` / `download-range`.
    // Standardmäßig DEFAULT_BGBL_JOBS und stets auf >= 1 begrenzt.
    int bgbl_jobs() const
    {
        int const value = bgblJobs_.value_or(DEFAULT_BGBL_JOBS);
        return value < 1 ? 1 : value;
    }

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
            auto const parsed_source = source_from_arg(arg);
            if (parsed_source.has_value()) {
                source_ = parsed_source.value();
                if (!customOutputDirectory_) {
                    outputDirectory_ = default_output_directory_for_source(source_);
                }
            }
            return true;
        }

        if (is_bgbl_year_arg(arg)) {
            bgblYear_ = parse_int_arg(arg, 7);
            return true;
        }
        if (is_bgbl_nr_arg(arg)) {
            // strlen("--nr=")
            bgblNrRaw_ = arg.substr(5);
            return true;
        }
        if (is_bgbl_part_arg(arg)) {
            bgblPart_ = parse_part_arg(arg);
            return true;
        }
        if (is_bgbl_from_arg(arg)) {
            bgblFrom_ = parse_int_arg(arg, 7);
            return true;
        }
        if (is_bgbl_to_arg(arg)) {
            bgblTo_ = parse_int_arg(arg, 5);
            return true;
        }
        if (is_bgbl_jobs_arg(arg)) {
            bgblJobs_ = parse_int_arg(arg, 7);
            return true;
        }

        if (is_bgbl_source_arg(arg)) {
            auto const parsed_source = parse_bgbl_source_arg(arg);
            if (parsed_source.has_value()) {
                bgblSource_ = parsed_source.value();
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

    static bool is_bgbl_year_arg(std::string_view const arg)
    { return starts_with(arg, "--year="); }

    static bool is_bgbl_nr_arg(std::string_view const arg)
    { return starts_with(arg, "--nr="); }

    static bool is_bgbl_part_arg(std::string_view const arg)
    { return starts_with(arg, "--part="); }

    static bool is_bgbl_from_arg(std::string_view const arg)
    { return starts_with(arg, "--from="); }

    static bool is_bgbl_to_arg(std::string_view const arg)
    { return starts_with(arg, "--to="); }

    static bool is_bgbl_jobs_arg(std::string_view const arg)
    { return starts_with(arg, "--jobs="); }

    static bool is_bgbl_source_arg(std::string_view const arg)
    { return starts_with(arg, "--bgbl-source="); }

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

    static std::optional<int> parse_int_arg(std::string const & arg, std::size_t const prefix_length)
    {
        auto const value_part = arg.substr(prefix_length);
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

    static std::optional<int> parse_part_arg(std::string const & arg)
    {
        // strlen("--part=")
        constexpr std::size_t prefix_length = 7;
        if (arg.size() <= prefix_length) {
            return std::nullopt;
        }

        auto const value_part = arg.substr(prefix_length);
        if (value_part == "1" || value_part == "I" || value_part == "i") {
            return 1;
        }
        if (value_part == "2" || value_part == "II" || value_part == "ii") {
            return 2;
        }

        return std::nullopt;
    }

    static std::optional<std::string> parse_bgbl_source_arg(std::string const & arg)
    {
        // strlen("--bgbl-source=")
        constexpr std::size_t prefix_length = 14;
        if (arg.size() <= prefix_length) {
            return std::nullopt;
        }

        auto const value_part = arg.substr(prefix_length);
        if (value_part == "xaver" || value_part == "og" || value_part == "offenegesetze" ||
            value_part == "offene-gesetze" || value_part == "ris" || value_part == "bund" || value_part == "bundris") {
            return value_part;
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

    // bgbl.de Download-Optionen
    std::optional<int> bgblYear_;
    std::optional<std::string> bgblNrRaw_;
    std::optional<int> bgblPart_;
    std::optional<int> bgblFrom_;
    std::optional<int> bgblTo_;
    std::optional<int> bgblJobs_;
    std::optional<std::string> bgblSource_;
};
