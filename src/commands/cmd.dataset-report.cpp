// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "commands/cmd.dataset-report.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "utils/dataset.h"

DatasetReportCommand::DatasetReportCommand(AppConfig& appConfig) : CliCommand(appConfig)
{
    // empty
}

void DatasetReportCommand::run(std::string_view const & command) const
{
    // not used
    (void)command;

    spdlog::info("Erzeuge Datensatz-Report");

    std::filesystem::path const output_root  = appConfig_.output_directory();
    std::filesystem::path const dataset_root = output_root / AppConfig::RAW_EXTRACTED_DIRECTORY;

    gdl::generate_manifest(output_root, appConfig_);
    gdl::generate_checksums(output_root, appConfig_);

    generateFileReport(output_root.string(), dataset_root.string());

    spdlog::info("Fertig");
}

void DatasetReportCommand::generateFileReport(std::string const & outputRoot, std::string const & datasetRoot) const
{
    auto const output_root  = std::filesystem::path(outputRoot).make_preferred();
    auto const dataset_root = std::filesystem::path(datasetRoot).make_preferred();

    auto const toc_path   = output_root / appConfig_.toc_file();
    bool const toc_exists = std::filesystem::exists(toc_path);

    bool const is_vvii = (appConfig_.source() == AppConfig::DownloadSource::Vvii);

    auto const count_files_recursive = [](std::filesystem::path const & root) -> int {
        if (!std::filesystem::exists(root)) {
            return 0;
        }
        auto const begin = std::filesystem::recursive_directory_iterator(root);
        auto const end   = std::filesystem::recursive_directory_iterator{};
        return static_cast<int>(std::count_if(begin, end, [](std::filesystem::directory_entry const & entry) {
            return entry.is_regular_file();
        }));
    };

    auto const count_lines = [](std::filesystem::path const & file) -> int {
        if (!std::filesystem::exists(file)) {
            return 0;
        }
        int lines = 0;
        std::ifstream in(file.string());
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty()) {
                lines++;
            }
        }
        return lines;
    };

    std::string report;

    report += format_status(
        "# Datensatz-Report für Quelle '" + appConfig_.source_name() + "' (" + get_datetime() + ")\n", 0, Color::Blue);

    report += format_status("\nDatenordner: " + output_root.string() + "\n", 0, Color::Blue);
    report += format_status("Pfad zur TOC-Datei: " + toc_path.string() + "\n", 0, Color::Blue);

    report += format_status("\n## Dateien und Ordner\n\n", 0, Color::Blue);

    std::vector<std::pair<std::string, int>> report_items;
    report_items.emplace_back("TOC-XML vorhanden: ", toc_exists ? 1 : 0);
    report_items.emplace_back("Manifest erzeugt: ", std::filesystem::exists(output_root / "manifest.json") ? 1 : 0);
    report_items.emplace_back("Checksums erzeugt: ", std::filesystem::exists(output_root / "checksums.sha256") ? 1 : 0);

    if (!is_vvii) {
        // GII: XML-Auswertung in raw-extracted
        std::unordered_map<std::string, std::pair<int, int>> folderFileCounts;
        int numFolders = 0;

        if (std::filesystem::exists(dataset_root)) {
            {
                auto const begin = std::filesystem::recursive_directory_iterator(dataset_root);
                auto const end   = std::filesystem::recursive_directory_iterator{};
                numFolders =
                    static_cast<int>(std::count_if(begin, end, [](std::filesystem::directory_entry const & entry) {
                        return entry.is_directory();
                    }));
            }
            for (auto const & entry : std::filesystem::recursive_directory_iterator(dataset_root)) {
                if (entry.is_regular_file()) {
                    auto const & path = entry.path().parent_path().string();
                    if (entry.path().extension() == ".xml") {
                        folderFileCounts[path].first++;
                    } else {
                        folderFileCounts[path].second++;
                    }
                }
            }
        } else {
            spdlog::warn("Datensatzordner für Report nicht gefunden: {}", dataset_root.string());
        }

        int numFoldersWithXmlFile              = 0;
        int numFoldersWithXmlFileAndOtherFiles = 0;
        int numFoldersWithoutXmlFile           = 0;

        std::string folder_report;
        folder_report += format_status("\n## Ordner mit XML-Datei und anderen Dateien\n\n", 0, Color::Blue);

        std::string folder_without_xml_report;
        folder_without_xml_report += format_status("\n## Ordner ohne XML-Datei\n\n", 0, Color::Blue);

        std::string folder_with_other_files_report;
        folder_with_other_files_report += format_status("\n## Ordner mit anderen Dateien\n\n", 0, Color::Blue);

        for (auto const & folderFileCount : folderFileCounts) {
            if (folderFileCount.second.first > 0) {
                numFoldersWithXmlFile++;
                if (folderFileCount.second.second > 0) {
                    numFoldersWithXmlFileAndOtherFiles++;
                    folder_report += folderFileCount.first + "\n";
                }
            } else {
                numFoldersWithoutXmlFile++;
                folder_without_xml_report += folderFileCount.first + "\n";
            }

            if (folderFileCount.second.second > 0) {
                folder_with_other_files_report += format_status(folderFileCount.first + "\n", 1, Color::Yellow);
                for (auto const & entry : std::filesystem::directory_iterator(folderFileCount.first)) {
                    if (entry.is_regular_file() && entry.path().extension() != ".xml") {
                        folder_with_other_files_report += "  " + entry.path().filename().string() + "\n";
                    }
                }
            }
        }

        report_items.emplace_back("Ordner: ", numFolders);
        report_items.emplace_back("Ordner mit XML-Datei: ", numFoldersWithXmlFile);
        report_items.emplace_back("Ordner mit XML-Datei und anderen Dateien: ", numFoldersWithXmlFileAndOtherFiles);
        report_items.emplace_back("Ordner ohne XML-Datei: ", numFoldersWithoutXmlFile);

        for (auto const & item : report_items) {
            report += format("{:<50} {:>5}\n", item.first, item.second);
        }

        report += folder_report;
        report += folder_without_xml_report;

        if (numFoldersWithoutXmlFile == 0) {
            report += "keine\n";
        }

        report += folder_with_other_files_report;
    } else {
        // VVII: Rohdaten in raw-pages, raw-artifacts und Crawl-State
        std::filesystem::path const raw_pages     = output_root / "raw-pages";
        std::filesystem::path const raw_artifacts = output_root / "raw-artifacts";
        std::filesystem::path const state_root    = output_root / "state";

        int const pages     = count_files_recursive(raw_pages);
        int const artifacts = count_files_recursive(raw_artifacts);

        report_items.emplace_back("Seiten (raw-pages): ", pages);
        report_items.emplace_back("Artefakte (raw-artifacts): ", artifacts);
        report_items.emplace_back("Frontier-Seiten: ", count_lines(state_root / "frontier-pages.txt"));
        report_items.emplace_back("Besuchte Seiten: ", count_lines(state_root / "visited-pages.txt"));
        report_items.emplace_back("Entdeckte Artefakte: ", count_lines(state_root / "discovered-artifacts.txt"));
        report_items.emplace_back("Heruntergeladene Artefakte: ", count_lines(state_root / "downloaded-artifacts.txt"));
        report_items.emplace_back("Fehlgeschlagene Artefakte: ", count_lines(state_root / "failed-artifacts.txt"));

        for (auto const & item : report_items) {
            report += format("{:<50} {:>5}\n", item.first, item.second);
        }
    }

    // write to console
    std::cout << report << "\n";

    report = remove_color_codes(report);

    // write to file
    std::filesystem::create_directories(output_root);
    std::ofstream file_out((output_root / AppConfig::DATASET_REPORT_FILE).string());
    file_out << report << "\n";
    file_out.close();
}
