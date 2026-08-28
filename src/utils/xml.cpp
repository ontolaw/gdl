// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "utils/xml.h"

#include <spdlog/spdlog.h>

#include <string>
#include <unordered_map>
#include <vector>

// Die gii-toc.xml-Datei parsen und die URLs extrahieren
std::vector<std::string> get_urls(std::string const & toc_file)
{
    std::vector<std::string> urls;

    // Dokument öffnen
    pugi::xml_document doc;
    pugi::xml_parse_result const result = doc.load_file(toc_file.c_str());
    if (!result) {
        spdlog::error("Fehler beim Parsen der XML-Datei {}: {}.", toc_file, result.description());
        return {};
    }

    // XML-Knoten-Traversierung -> url = "items.item.link"
    pugi::xml_node const root = doc.child("items");
    if (!root) {
        spdlog::error("Fehler beim Parsen der XML-Datei {}.", toc_file);
        return {};
    }
    for (pugi::xml_node node = root.child("item"); node != nullptr; node = node.next_sibling("item")) {
        std::string url;
        for (pugi::xml_node child = node.first_child(); child != nullptr; child = child.next_sibling()) {
            if (std::string(child.name()) == "link") {
                url = child.child_value();
                break;
            }
        }
        if (!url.empty()) {
            urls.push_back(url);
        }
    }

    return urls;
}

void format_xml_files(std::string const & folder)
{
    std::filesystem::path root_path(folder);

    // CWD anhängen, falls relativer Pfad
    if (root_path.is_relative()) {
        root_path = std::filesystem::absolute(root_path);
    }

    ensure_writeable(folder);

    if (!std::filesystem::exists(root_path)) {
        spdlog::error("Ordner nicht gefunden: {}", root_path.string());
        spdlog::debug("cwd: {}", std::filesystem::current_path().string());
        return;
    }

    // Abbildung von "old_node_name" auf "new_node_name"
    /*std::unordered_map<std::string, std::string> node_rename_map = {

        // https://www.gesetze-im-internet.de/dtd/1.01/gii-norm.dtd
        // Raten bei nicht dokumentierten, mangelhaft spezifizierten Bezeichnern

        // Metadaten
        {"jurabk", "abkuerzung_jur"},
        {"amtabk", "abkuerzung_amt"},
        // langüberschrift ?
        {"langue", "titel_lang"},
        // kurzüberschrift ?
        {"kurzue", "titel_kurz"},
        // ausfertigung-datum
        {"ausfertigung-datum", "ausfertigungsdatum"}, // ausgefertigt-am



    };*/

    for (auto const & entry : std::filesystem::recursive_directory_iterator(root_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".xml") {
            std::string file_name = entry.path().string();
            pugi::xml_document doc;
            if (doc.load_file(file_name.c_str())) {
                spdlog::info("Formatiere {}", file_name);

                // rename_xml_nodes(doc, node_rename_map);

                std::ofstream outfile(file_name, std::ios::binary);
                doc.save(outfile, "  ", pugi::format_default | pugi::format_indent);
                outfile.close();
            } else {
                spdlog::error("Fehler beim Laden der Datei {}", file_name);
            }
        }
    }
}

void rename_xml_nodes(pugi::xml_document& doc, std::unordered_map<std::string, std::string> const & node_map)
{
    // Mehrere Knoten umbenennen
    for (auto const& [old_name, new_name] : node_map) {
        // Knoten finden
        pugi::xml_node node = doc.child(old_name.c_str());
        if (node.empty()) {
            spdlog::warn("Knoten {} nicht in XML-Datei gefunden", old_name);
            continue;
        }

        // Knoten umbenennen
        node.set_name(new_name.c_str());

        spdlog::info("Knoten {} erfolgreich in {} umbenannt", old_name, new_name);
    }
}
