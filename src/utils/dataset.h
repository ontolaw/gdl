// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <filesystem>

#include "app/config.h"

// Erzeugt die Snapshot-Metadaten des aktuellen Laufs:
// - manifest.json (Laufbeschreibung)
// - checksums.sha256 (Integritaets-Hashes aller Artefakte)
namespace gdl
{
    void generate_manifest(std::filesystem::path const & outputRoot, AppConfig const & appConfig);
    void generate_checksums(std::filesystem::path const & outputRoot, AppConfig const & appConfig);
} // namespace gdl
