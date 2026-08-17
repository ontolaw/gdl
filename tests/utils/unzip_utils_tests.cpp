// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "minizip/zip.h"
#include "utils/unzip.h"

int unzip_file(char const * zip_filepath, char const * target_folder);

namespace
{
    bool create_zip_with_single_file(
        std::filesystem::path const & zip_path, std::string const & inner_name, std::string const & content)
    {
        zipFile zip = zipOpen(zip_path.string().c_str(), APPEND_STATUS_CREATE);
        if (zip == nullptr) {
            return false;
        }

        zip_fileinfo const info{};
        int err = zipOpenNewFileInZip(
            zip, inner_name.c_str(), &info, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED, Z_DEFAULT_COMPRESSION);
        if (err != ZIP_OK) {
            zipClose(zip, nullptr);
            return false;
        }

        err = zipWriteInFileInZip(zip, content.data(), static_cast<unsigned int>(content.size()));
        if (err != ZIP_OK) {
            zipCloseFileInZip(zip);
            zipClose(zip, nullptr);
            return false;
        }

        zipCloseFileInZip(zip);
        zipClose(zip, nullptr);
        return true;
    }
} // namespace

TEST_CASE("unzip_file fails gracefully for non-existing archive", "[unzip]")
{ REQUIRE(unzip_file("/definitely/not/found/archive.zip", ".") == -1); }

TEST_CASE("unzip_file fails for empty archive without entries", "[unzip]")
{
    auto const temp_root = std::filesystem::temp_directory_path() / "gdl_unzip_empty_archive";
    auto const zip_path  = temp_root / "empty.zip";

    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(temp_root);

    zipFile zip = zipOpen(zip_path.string().c_str(), APPEND_STATUS_CREATE);
    REQUIRE(zip != nullptr);
    zipClose(zip, nullptr);

    REQUIRE(unzip_file(zip_path.string().c_str(), temp_root.string().c_str()) == -1);

    std::filesystem::remove_all(temp_root);
}

TEST_CASE("extract_zips extracts XML and removes ZIP", "[unzip]")
{
    auto const temp_root = std::filesystem::temp_directory_path() / "gdl_unzip_utils_ok";
    auto const target    = temp_root / "pkg";
    auto const zip_path  = target / "sample.zip";
    auto const xml_path  = target / "sample.xml";

    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(target);

    REQUIRE(create_zip_with_single_file(zip_path, "sample.xml", "<root><x>1</x></root>"));

    REQUIRE(extract_zips(temp_root.string()) == EXIT_SUCCESS);
    REQUIRE(std::filesystem::exists(xml_path));
    REQUIRE_FALSE(std::filesystem::exists(zip_path));

    std::filesystem::remove_all(temp_root);
}

TEST_CASE("extract_zips returns failure on invalid zip payload", "[unzip]")
{
    auto const temp_root = std::filesystem::temp_directory_path() / "gdl_unzip_utils_bad";
    auto const bad_zip   = temp_root / "broken.zip";

    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(temp_root);

    {
        std::ofstream out(bad_zip.string());
        out << "not a zip";
    }

    REQUIRE(extract_zips(temp_root.string()) == EXIT_FAILURE);
    REQUIRE(std::filesystem::exists(bad_zip));

    std::filesystem::remove_all(temp_root);
}

TEST_CASE("extract_zips_to_folder extracts into separate target and keeps ZIP", "[unzip]")
{
    auto const temp_root      = std::filesystem::temp_directory_path() / "gdl_unzip_utils_split";
    auto const source_root    = temp_root / "raw-zips";
    auto const target_root    = temp_root / "raw-extracted";
    auto const package_folder = source_root / "pkg";
    auto const zip_path       = package_folder / "sample.zip";
    auto const xml_path       = target_root / "pkg" / "sample.xml";

    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(package_folder);

    REQUIRE(create_zip_with_single_file(zip_path, "sample.xml", "<root><x>1</x></root>"));

    REQUIRE(extract_zips_to_folder(source_root.string(), target_root.string(), false) == EXIT_SUCCESS);
    REQUIRE(std::filesystem::exists(xml_path));
    REQUIRE(std::filesystem::exists(zip_path));

    std::filesystem::remove_all(temp_root);
}

TEST_CASE("unzip_file fails when target output folder does not exist", "[unzip]")
{
    auto const temp_root      = std::filesystem::temp_directory_path() / "gdl_unzip_target_missing";
    auto const zip_path       = temp_root / "sample.zip";
    auto const missing_target = temp_root / "missing";

    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(temp_root);

    REQUIRE(create_zip_with_single_file(zip_path, "sample.xml", "<root/>"));

    REQUIRE(unzip_file(zip_path.string().c_str(), missing_target.string().c_str()) == -1);

    std::filesystem::remove_all(temp_root);
}
