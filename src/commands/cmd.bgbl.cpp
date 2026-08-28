// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "commands/cmd.bgbl.h"

#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "utils/bgbl.h"
#include "utils/bgbl_og.h"
#include "utils/bgbl_ris.h"

namespace
{
    // Normalisiert den --bgbl-source=-Wert zu einem Backend-Selektor.
    enum class BgblBackend : std::uint8_t
    {
        Xaver,
        Og,
        Ris
    };

    BgblBackend backend_from_source(std::string const & source)
    {
        if (source == "og" || source == "offenegesetze" || source == "offene-gesetze") {
            return BgblBackend::Og;
        }
        if (source == "ris" || source == "bund" || source == "bundris") {
            return BgblBackend::Ris;
        }
        return BgblBackend::Xaver;
    }
} // namespace

BgblCommand::BgblCommand(AppConfig& appConfig) : CliCommand(appConfig)
{
    // empty
}

void BgblCommand::run(std::string_view const & command) const
{
    // not used
    (void)command;

    auto& config       = appConfig_;
    int const part     = config.bgbl_part().value_or(1);
    auto const out_dir = config.output_directory();
    auto const backend = backend_from_source(config.bgbl_source());

    try {
        if (command == "download-toc") {
            auto const year = config.bgbl_year();
            if (!year) {
                throw std::runtime_error("download-toc benötigt --year=.");
            }
            if (backend == BgblBackend::Og) {
                bgbl::og::download_toc(part, *year, config.is_very_verbose());
            } else if (backend == BgblBackend::Ris) {
                bgbl::ris::download_toc(part, *year, config.is_very_verbose());
            } else {
                bgbl::download_toc(part, *year, config.is_very_verbose());
            }
        } else if (command == "download-issue") {
            auto const year   = config.bgbl_year();
            auto const nr_raw = config.bgbl_nr_raw();
            auto const nr     = config.bgbl_nr();
            if (!year || !nr_raw || nr_raw->empty()) {
                throw std::runtime_error("download-issue benötigt --year= und --nr=.");
            }
            if (backend == BgblBackend::Og) {
                if (!nr) {
                    throw std::runtime_error(
                        "download-issue benötigt eine numerische --nr= für die OffeneGesetze.de-Quelle.");
                }
                bgbl::og::download_issue(part, *year, *nr, out_dir, config.is_very_verbose());
            } else if (backend == BgblBackend::Ris) {
                bgbl::ris::download_issue(part, *year, *nr_raw, out_dir, config.is_very_verbose());
            } else {
                if (!nr) {
                    throw std::runtime_error("download-issue benötigt eine numerische --nr= für die bgbl.de-Quelle.");
                }
                bgbl::download_issue(part, *year, *nr, out_dir, config.is_very_verbose());
            }
        } else if (command == "download-year") {
            auto const year = config.bgbl_year();
            if (!year) {
                throw std::runtime_error("download-year benötigt --year=.");
            }
            if (backend == BgblBackend::Og) {
                bgbl::og::download_year(part, *year, out_dir, config.is_very_verbose(), config.bgbl_jobs());
            } else if (backend == BgblBackend::Ris) {
                bgbl::ris::download_year(part, *year, out_dir, config.is_very_verbose(), config.bgbl_jobs());
            } else {
                bgbl::download_year(part, *year, out_dir, config.is_very_verbose(), config.bgbl_jobs());
            }
        } else if (command == "download-range") {
            auto const from = config.bgbl_from();
            auto const to   = config.bgbl_to();
            if (!from || !to) {
                throw std::runtime_error("download-range benötigt --from= und --to=.");
            }
            if (backend == BgblBackend::Og) {
                bgbl::og::download_range(part, *from, *to, out_dir, config.is_very_verbose(), config.bgbl_jobs());
            } else if (backend == BgblBackend::Ris) {
                bgbl::ris::download_range(part, *from, *to, out_dir, config.is_very_verbose(), config.bgbl_jobs());
            } else {
                bgbl::download_range(part, *from, *to, out_dir, config.is_very_verbose(), config.bgbl_jobs());
            }
        } else {
            spdlog::error("Unbekannter BGBl-Befehl: {}", command);
            exit(EXIT_FAILURE);
        }
    } catch (std::exception const & e) {
        spdlog::error("BGBl-Download fehlgeschlagen: {}", e.what());
        exit(EXIT_FAILURE);
    }
}
