// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "commands/cmd.dataset-report.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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

    generateFileReport(output_root.string(), dataset_root.string());

    spdlog::info("Fertig");
}

void DatasetReportCommand::generateFileReport(std::string const & outputRoot, std::string const & datasetRoot) const
{
    auto const output_root  = std::filesystem::path(outputRoot).make_preferred();
    auto const dataset_root = std::filesystem::path(datasetRoot).make_preferred();

    // Create a map to store the count of XML files and other files in each folder
    std::unordered_map<std::string, std::pair<int, int>> folderFileCounts;

    int numFolders = 0;

    auto const toc_path   = output_root / appConfig_.toc_file();
    bool const toc_exists = std::filesystem::exists(toc_path);

    // Traverse the directory tree and count the XML files and other files in each
    // folder
    if (std::filesystem::exists(dataset_root)) {
        for (auto const & entry : std::filesystem::recursive_directory_iterator(dataset_root)) {
            // number of folders
            if (entry.is_directory()) {
                numFolders++;
            }

            if (entry.is_regular_file()) {
                auto const & path = entry.path().parent_path().string();
                if (entry.path().extension() == ".xml") {
                    // Increment the count of XML files in the folder containing this file
                    folderFileCounts[path].first++;
                } else {
                    // Increment the count of other files in the folder containing this
                    // file
                    folderFileCounts[path].second++;
                }
            }
        }
    } else {
        spdlog::warn("Datensatzordner für Report nicht gefunden: {}", dataset_root.string());
    }

    // Generate the report

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
                // Folder with XML file and other files
                numFoldersWithXmlFileAndOtherFiles++;
                folder_report += folderFileCount.first + "\n";
            }
        } else {
            // Folder without XML file
            numFoldersWithoutXmlFile++;
            folder_without_xml_report += folderFileCount.first + "\n";
        }

        if (folderFileCount.second.second > 0) {
            // Folder with other files
            folder_with_other_files_report += format_status(folderFileCount.first + "\n", 1, Color::Yellow);
            for (auto const & entry : std::filesystem::directory_iterator(folderFileCount.first)) {
                if (entry.is_regular_file() && entry.path().extension() != ".xml") {
                    folder_with_other_files_report += "  " + entry.path().filename().string() + "\n";
                }
            }
        }
    }

    // Generate Report

    std::string report;

    report += format_status(
        "# Datensatz-Report für Quelle '" + appConfig_.source_name() + "' (" + get_datetime() + ")\n", 0, Color::Blue);

    report += format_status("\nDatenordner: " + dataset_root.string() + "\n", 0, Color::Blue);
    report += format_status("Pfad zur TOC-Datei: " + toc_path.string() + "\n", 0, Color::Blue);

    report += format_status("\n## Dateien und Ordner\n\n", 0, Color::Blue);

    std::vector<std::pair<std::string, int>> report_items;
    report_items.emplace_back("TOC-XML vorhanden: ", toc_exists ? 1 : 0);
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

    // write to console
    std::cout << report << "\n";

    report = remove_color_codes(report);

    // write to file
    std::filesystem::create_directories(output_root);
    std::ofstream file_out((output_root / AppConfig::DATASET_REPORT_FILE).string());
    file_out << report << "\n";
    file_out.close();
}
