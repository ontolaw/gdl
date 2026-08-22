// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include "utils/dataset.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <openssl/evp.h>
#include <sstream>
#include <string>
#include <vector>

// use std::format on MSVC - use libfmt as polyfill on Linux
#ifdef _WIN32
#include <format>
using std::format;
#else
// #include <fmt/core.h>
#include <fmt/format.h>
using fmt::format;
#endif

#include "version.h"

namespace gdl
{
    namespace
    {
        std::string utc_now_iso()
        {
            auto const now = std::chrono::system_clock::now();
            auto const t   = std::chrono::system_clock::to_time_t(now);
            std::tm tm_utc{};
#ifdef _WIN32
            gmtime_s(&tm_utc, &t);
#else
            gmtime_r(&t, &tm_utc);
#endif
            auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

            std::ostringstream oss;
            oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%S");
            oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << "Z";
            return oss.str();
        }

        int count_files(std::filesystem::path const & root)
        {
            if (!std::filesystem::exists(root)) {
                return 0;
            }
            auto const begin = std::filesystem::recursive_directory_iterator(root);
            auto const end   = std::filesystem::recursive_directory_iterator{};
            return static_cast<int>(std::count_if(begin, end, [](std::filesystem::directory_entry const & entry) {
                return entry.is_regular_file();
            }));
        }

        std::string compute_sha256_hex(std::filesystem::path const & path)
        {
            std::ifstream in(path.string(), std::ios::binary);
            if (!in) {
                return std::string{};
            }

            EVP_MD_CTX* ctx = EVP_MD_CTX_new();
            EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);

            std::array<char, 65536> buffer{};
            while (in.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) || in.gcount() > 0) {
                EVP_DigestUpdate(ctx, buffer.data(), static_cast<std::size_t>(in.gcount()));
            }

            std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
            unsigned int digest_len = 0;
            EVP_DigestFinal_ex(ctx, digest.data(), &digest_len);
            EVP_MD_CTX_free(ctx);

            std::ostringstream oss;
            oss << std::hex << std::setfill('0');
            for (unsigned int i = 0; i < digest_len; ++i) {
                oss << std::setw(2) << static_cast<int>(digest[i]);
            }
            return oss.str();
        }
    } // namespace

    void generate_manifest(std::filesystem::path const & outputRoot, AppConfig const & appConfig)
    {
        auto root = outputRoot;
        root.make_preferred();
        bool const is_vvii = (appConfig.source() == AppConfig::DownloadSource::Vvii);

        std::ostringstream counts;
        if (!is_vvii) {
            int const raw_zips          = count_files(root / AppConfig::RAW_ZIPS_DIRECTORY);
            int const raw_extracted_xml = ([&root]() {
                auto const ex = root / AppConfig::RAW_EXTRACTED_DIRECTORY;
                if (!std::filesystem::exists(ex)) {
                    return 0;
                }
                auto const begin = std::filesystem::recursive_directory_iterator(ex);
                auto const end   = std::filesystem::recursive_directory_iterator{};
                return static_cast<int>(std::count_if(begin, end, [](std::filesystem::directory_entry const & e) {
                    return e.is_regular_file() && e.path().extension() == ".xml";
                }));
            })();
            counts << format("  \"raw_zips\": {},", raw_zips) << "\n";
            counts << format("  \"raw_extracted_xml\": {}", raw_extracted_xml);
        } else {
            int const raw_pages     = count_files(root / "raw-pages");
            int const raw_artifacts = count_files(root / "raw-artifacts");
            counts << format("  \"raw_pages\": {},", raw_pages) << "\n";
            counts << format("  \"raw_artifacts\": {}", raw_artifacts);
        }

        std::string const manifest = format(
            "{{\n"
            "  \"run_timestamp\": \"{}\",\n"
            "  \"source\": \"{}\",\n"
            "  \"source_urls\": [\n"
            "    \"{}\"\n"
            "  ],\n"
            "  \"tool_version\": \"{}\",\n"
            "  \"checksum_algorithm\": \"sha256\",\n"
            "  \"file_counts\": {{\n"
            "{}\n"
            "  }}\n"
            "}}\n",
            utc_now_iso(),
            appConfig.source_name(),
            appConfig.toc_url(),
            app_version::get_version(),
            counts.str());

        std::filesystem::create_directories(root);
        std::ofstream out((root / "manifest.json").string());
        out << manifest;
    }

    void generate_checksums(std::filesystem::path const & outputRoot, AppConfig const & appConfig)
    {
        auto root = outputRoot;
        root.make_preferred();
        bool const is_vvii = (appConfig.source() == AppConfig::DownloadSource::Vvii);

        std::vector<std::filesystem::path> roots;
        if (!is_vvii) {
            roots.push_back(root / AppConfig::RAW_ZIPS_DIRECTORY);
            roots.push_back(root / AppConfig::RAW_EXTRACTED_DIRECTORY);
        } else {
            roots.push_back(root / "raw-pages");
            roots.push_back(root / "raw-artifacts");
        }

        std::filesystem::create_directories(root);
        std::ofstream out((root / "checksums.sha256").string());

        for (auto const & r : roots) {
            if (!std::filesystem::exists(r)) {
                continue;
            }
            for (auto const & entry : std::filesystem::recursive_directory_iterator(r)) {
                if (!entry.is_regular_file()) {
                    continue;
                }
                std::string const hash          = compute_sha256_hex(entry.path());
                std::filesystem::path const rel = entry.path().lexically_relative(root);
                std::string rel_str             = rel.string();
                std::replace(rel_str.begin(), rel_str.end(), '\\', '/');
                out << hash << "  " << rel_str << "\n";
            }
        }
    }
} // namespace gdl
