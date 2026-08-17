// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "utils/unzip.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    struct FileCloser
    {
        void operator()(FILE* fp) const
        {
            if (fp != nullptr) {
                // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
                std::fclose(fp);
            }
        }
    };
} // namespace

int unzip_file(char const * zip_filepath, char const * target_folder)
{
    unzFile zip_file = unzOpen(zip_filepath);
    if (zip_file == nullptr) {
        spdlog::error("ZIP-Datei konnte nicht geöffnet werden: {}", zip_filepath);
        return -1;
    }
    if (unzGoToFirstFile(zip_file) != UNZ_OK) {
        spdlog::error("Erste Datei im ZIP konnte nicht geöffnet werden.");
        unzClose(zip_file);
        return -1;
    }
    for (;;) {
        std::array<char, 256> filename_in_zip{};
        unz_file_info file_info;
        if (unzGetCurrentFileInfo(
                zip_file,
                &file_info,
                filename_in_zip.data(),
                static_cast<uLong>(filename_in_zip.size()),
                nullptr,
                0,
                nullptr,
                0) != UNZ_OK) {
            spdlog::error("Dateiinformationen im ZIP konnten nicht gelesen werden.");
            unzClose(zip_file);
            return -1;
        }
        if (unzOpenCurrentFile(zip_file) != UNZ_OK) {
            spdlog::error("Aktuelle Datei im ZIP konnte nicht geöffnet werden.");
            unzClose(zip_file);
            return -1;
        }
        std::vector<char> xml_data(file_info.uncompressed_size);
        int const read_size =
            unzReadCurrentFile(zip_file, xml_data.data(), static_cast<unsigned int>(file_info.uncompressed_size));
        if (std::cmp_not_equal(read_size, file_info.uncompressed_size)) {
            spdlog::error("XML-Datei konnte nicht vollständig aus dem ZIP gelesen werden.");
            unzCloseCurrentFile(zip_file);
            unzClose(zip_file);
            return -1;
        }
        std::string dest_path = std::string(target_folder) + "/" + std::string(filename_in_zip.data());
        std::unique_ptr<FILE, FileCloser> xml_file;
#if defined(_MSC_VER) && (defined(_WIN32) || defined(_WIN64))
        FILE* raw_xml_file = nullptr;
        fopen_s(&raw_xml_file, dest_path.c_str(), "wb");
        xml_file.reset(raw_xml_file);
#else
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        xml_file.reset(fopen(dest_path.c_str(), "wb"));
#endif
        if (xml_file == nullptr) {
            spdlog::error("XML-Datei konnte nicht zum Schreiben geöffnet werden: {}", dest_path);
            unzCloseCurrentFile(zip_file);
            unzClose(zip_file);
            return -1;
        }
        fwrite(xml_data.data(), 1, file_info.uncompressed_size, xml_file.get());
        unzCloseCurrentFile(zip_file);
        if (unzGoToNextFile(zip_file) != UNZ_OK) {
            break;
        }
    }
    unzClose(zip_file);
    return 0;
}

int extract_zips(std::string const & top_level_folder)
{ return extract_zips_to_folder(top_level_folder, top_level_folder, true); }

int extract_zips_to_folder(std::string const & source_root, std::string const & target_root, bool delete_source_zips)
{
    if (!std::filesystem::exists(source_root)) {
        return EXIT_SUCCESS;
    }

    // iterate over all folders in the source root
    for (auto const & entry : std::filesystem::recursive_directory_iterator(source_root)) {
        // check, if the folder contains a zip file
        if (entry.is_regular_file() && entry.path().extension() == ".zip") {
            auto const relative_parent = std::filesystem::relative(entry.path().parent_path(), source_root);
            auto const target_folder   = (std::filesystem::path(target_root) / relative_parent).make_preferred();
            std::filesystem::create_directories(target_folder);

            // unzip the file into the target folder
            if (unzip_file(entry.path().string().c_str(), target_folder.string().c_str()) != 0) {
                spdlog::error("ZIP-Datei konnte nicht entpackt werden: {}", entry.path().string());
                return EXIT_FAILURE;
            }

            if (!delete_source_zips) {
                continue;
            }

            // Check if an XML file was unzipped
            bool const xml_file_exists = std::any_of(
                std::filesystem::directory_iterator(target_folder),
                std::filesystem::directory_iterator(),
                [](std::filesystem::directory_entry const & file_entry) {
                    return file_entry.is_regular_file() && file_entry.path().extension() == ".xml";
                });

            // then delete the ZIP
            if (xml_file_exists) {
                std::filesystem::remove(entry.path());
            }
        }
    }

    return EXIT_SUCCESS;
}
