// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "utils/files.h"

#include <string>

void ensure_writeable(std::filesystem::path const & folder)
{
    // Berechtigungen des Verzeichnisses prüfen
    std::filesystem::file_status const status = std::filesystem::status(folder);
    std::filesystem::perms const permissions  = status.permissions();

    // Falls das Verzeichnis schreibgeschützt ist, Schreibberechtigung rekursiv anwenden
    if ((permissions & std::filesystem::perms::owner_write) == std::filesystem::perms::none) {
        for (auto const & p : std::filesystem::recursive_directory_iterator(folder)) {
            std::filesystem::permissions(p.path(), std::filesystem::perms::all);
        }
    }
}

std::string remove_leading_slash(std::string const & str)
{
    std::string result = str;
    if (!result.empty() && result.front() == '/') {
        result.erase(0, 1);
    }
    return result;
}

std::string remove_color_codes(std::string const & input_text)
{
    // Regulären Ausdruck definieren, um Farbcodes zu erkennen
    std::regex const color_regex("\x1b\\[[0-9;]+m");

    // Alle Farbcodes aus dem Eingabetext entfernen
    std::string filtered_text = std::regex_replace(input_text, color_regex, "");

    return filtered_text;
}
