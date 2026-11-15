// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>

namespace test_helpers
{
    class TempDir
    {
    public:
        explicit TempDir(
            std::string const & prefix, std::filesystem::path base = std::filesystem::temp_directory_path()) :
            path_(create(prefix, base))
        {
        }

        ~TempDir()
        {
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        }

        TempDir(TempDir const &)            = delete;
        TempDir& operator=(TempDir const &) = delete;
        TempDir(TempDir&&)                  = delete;
        TempDir& operator=(TempDir&&)       = delete;

        [[nodiscard]] std::filesystem::path const & path() const noexcept
        { return path_; }

    private:
        static std::filesystem::path create(std::string const & prefix, std::filesystem::path const & base)
        {
            std::filesystem::create_directories(base);

            static std::atomic_size_t sequence{0};
            auto const timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
            for (;;) {
                auto const candidate = base / (prefix + "_" + std::to_string(timestamp) + "_" +
                                               std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)));
                std::error_code error;
                if (std::filesystem::create_directory(candidate, error)) {
                    return candidate;
                }
                if (error) {
                    throw std::filesystem::filesystem_error("Failed to create temporary directory", candidate, error);
                }
            }
        }

        std::filesystem::path path_;
    };
} // namespace test_helpers
