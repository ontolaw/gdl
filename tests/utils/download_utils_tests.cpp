// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include <curl/curl.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "utils/download.h"

bool env_flag_enabled(char const * name);
std::string shorten_filename(std::string const & filename, std::size_t max_len = 60);
std::string join_filenames(std::vector<std::string> const & files, std::size_t max_items);
std::string format_duration(std::chrono::seconds duration);
DownloadOutputMode determine_output_mode(bool curlVerbose);
std::vector<CURL*> setup_curl_handles(
    std::vector<std::string>& urls,
    std::vector<CURL*>& curl_handles,
    std::vector<std::unique_ptr<FILE, FileCloser>>& files,
    std::vector<bool>& scheduled,
    int& setup_failures,
    bool curlVerbose,
    std::filesystem::path const & outputRoot);
CURLM* setup_curl_multi_handle(std::vector<CURL*> const & curl_easy_handles);
std::vector<std::string> collect_active_files(
    std::vector<CURL*> const & curl_easy_handles, std::vector<bool> const & transfer_done);
void render_interactive_status(
    const struct TestDownloadProgressState& state, std::vector<std::string> const & active_files);
void maybe_render_compact_status(
    struct TestDownloadProgressState& state, std::vector<std::string> const & active_files, bool force = false);
void render_download_status(
    struct TestDownloadProgressState& state, std::vector<std::string> const & active_files, bool force = false);

struct TestDownloadProgressState
{
    int total                                                 = 0;
    int finished                                              = 0;
    int succeeded                                             = 0;
    int failed                                                = 0;
    DownloadOutputMode output_mode                            = DownloadOutputMode::Interactive;
    std::chrono::steady_clock::time_point started_at          = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point last_compact_log_at = started_at;
    int last_compact_log_finished                             = 0;
    bool interactive_rendered                                 = false;
};

namespace
{
    struct private_data
    {
        int num_url = 0;
    };

    class CurrentPathGuard
    {
    public:
        explicit CurrentPathGuard(std::filesystem::path const & next) : previous_(std::filesystem::current_path())
        { std::filesystem::current_path(next); }

        ~CurrentPathGuard()
        { std::filesystem::current_path(previous_); }

        CurrentPathGuard(CurrentPathGuard const &)            = delete;
        CurrentPathGuard& operator=(CurrentPathGuard const &) = delete;
        CurrentPathGuard(CurrentPathGuard&&)                  = delete;
        CurrentPathGuard& operator=(CurrentPathGuard&&)       = delete;

    private:
        std::filesystem::path previous_;
    };

#ifndef _WIN32
    class LocalHttpFixture
    {
    public:
        explicit LocalHttpFixture(std::string body) :
            server_fd_(::socket(AF_INET, SOCK_STREAM, 0)), body_(std::move(body))
        {
            if (server_fd_ < 0) {
                throw std::runtime_error("socket() failed");
            }

            int const reuse_addr = 1;
            if (::setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) < 0) {
                ::close(server_fd_);
                server_fd_ = -1;
                throw std::runtime_error("setsockopt() failed");
            }

            sockaddr_in addr{};
            addr.sin_family      = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port        = 0;

            if (::bind(
                    server_fd_,
                    // NOLINTNEXTLINE(bugprone-casting-through-void)
                    static_cast<sockaddr*>(static_cast<void*>(&addr)),
                    sizeof(addr)) < 0) {
                ::close(server_fd_);
                server_fd_ = -1;
                throw std::runtime_error("bind() failed");
            }

            socklen_t len = sizeof(addr);
            if (::getsockname(
                    server_fd_,
                    // NOLINTNEXTLINE(bugprone-casting-through-void)
                    static_cast<sockaddr*>(static_cast<void*>(&addr)),
                    &len) < 0) {
                ::close(server_fd_);
                server_fd_ = -1;
                throw std::runtime_error("getsockname() failed");
            }
            port_ = ntohs(addr.sin_port);

            if (::listen(server_fd_, 1) < 0) {
                ::close(server_fd_);
                server_fd_ = -1;
                throw std::runtime_error("listen() failed");
            }

            server_thread_ = std::thread([this]() {
                sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);
                int const client_fd  = ::accept(
                    server_fd_,
                    // NOLINTNEXTLINE(bugprone-casting-through-void)
                    static_cast<sockaddr*>(static_cast<void*>(&client_addr)),
                    &client_len);
                if (client_fd < 0) {
                    return;
                }

                std::array<char, 512> request_buffer{};
                (void)::recv(client_fd, request_buffer.data(), request_buffer.size(), 0);

                std::string const response =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/octet-stream\r\n"
                    "Content-Length: " +
                    std::to_string(body_.size()) +
                    "\r\n"
                    "Connection: close\r\n\r\n" +
                    body_;

                std::size_t sent_total = 0;
                while (sent_total < response.size()) {
                    ssize_t const sent =
                        ::send(client_fd, response.substr(sent_total).data(), response.size() - sent_total, 0);
                    if (sent <= 0) {
                        break;
                    }
                    sent_total += static_cast<std::size_t>(sent);
                }

                ::shutdown(client_fd, SHUT_RDWR);
                ::close(client_fd);
            });
        }

        ~LocalHttpFixture()
        {
            if (server_fd_ >= 0) {
                ::shutdown(server_fd_, SHUT_RDWR);
                ::close(server_fd_);
                server_fd_ = -1;
            }

            if (server_thread_.joinable()) {
                server_thread_.join();
            }
        }

        LocalHttpFixture(LocalHttpFixture const &)            = delete;
        LocalHttpFixture& operator=(LocalHttpFixture const &) = delete;
        LocalHttpFixture(LocalHttpFixture&&)                  = delete;
        LocalHttpFixture& operator=(LocalHttpFixture&&)       = delete;

        [[nodiscard]] std::string url() const
        { return "http://127.0.0.1:" + std::to_string(port_) + "/fixture.bin"; }

    private:
        int server_fd_ = -1;
        uint16_t port_ = 0;
        std::thread server_thread_;
        std::string body_;
    };
#endif

    void set_env_var(char const * name, char const * value)
    {
#ifdef _MSC_VER
        _putenv_s(name, value);
#else
        setenv(name, value, 1);
#endif
    }

    void unset_env_var(char const * name)
    {
#ifdef _MSC_VER
        _putenv_s(name, "");
#else
        unsetenv(name);
#endif
    }
} // namespace

TEST_CASE("env_flag_enabled recognizes false-like and truthy values", "[download][utils]")
{
    unset_env_var("GDL_TEST_FLAG");
    REQUIRE_FALSE(env_flag_enabled("GDL_TEST_FLAG"));

    set_env_var("GDL_TEST_FLAG", "");
    REQUIRE_FALSE(env_flag_enabled("GDL_TEST_FLAG"));

    set_env_var("GDL_TEST_FLAG", "0");
    REQUIRE_FALSE(env_flag_enabled("GDL_TEST_FLAG"));

    set_env_var("GDL_TEST_FLAG", "false");
    REQUIRE_FALSE(env_flag_enabled("GDL_TEST_FLAG"));

    set_env_var("GDL_TEST_FLAG", "FALSE");
    REQUIRE_FALSE(env_flag_enabled("GDL_TEST_FLAG"));

    set_env_var("GDL_TEST_FLAG", "1");
    REQUIRE(env_flag_enabled("GDL_TEST_FLAG"));

    set_env_var("GDL_TEST_FLAG", "yes");
    REQUIRE(env_flag_enabled("GDL_TEST_FLAG"));

    unset_env_var("GDL_TEST_FLAG");
}

TEST_CASE("shorten_filename shortens only when needed", "[download][utils]")
{
    REQUIRE(shorten_filename("abc", 10) == "abc");

    auto const shortened = shorten_filename("abcdefghijklmnopqrstuvwxyz", 10);
    REQUIRE(shortened.size() == 10);
    REQUIRE(shortened.contains("..."));
}

TEST_CASE("join_filenames joins and truncates list", "[download][utils]")
{
    REQUIRE(join_filenames({}, 3) == "-");

    std::vector<std::string> const files{"first.xml", "second.xml", "third.xml", "fourth.xml"};
    REQUIRE(join_filenames(files, 2) == "first.xml | second.xml | +2 more");
    REQUIRE(join_filenames(files, 10) == "first.xml | second.xml | third.xml | fourth.xml");
}

TEST_CASE("format_duration creates human readable durations", "[download][utils]")
{
    REQUIRE(format_duration(std::chrono::seconds(5)) == "5s");
    REQUIRE(format_duration(std::chrono::seconds(65)) == "1m5s");
    REQUIRE(format_duration(std::chrono::seconds(3661)) == "1h1m1s");
}

TEST_CASE("determine_output_mode is compact for verbose and CI", "[download][utils]")
{
    REQUIRE(determine_output_mode(true) == DownloadOutputMode::Compact);

    unset_env_var("CI");
    unset_env_var("GITHUB_ACTIONS");
    set_env_var("CI", "true");

    REQUIRE(determine_output_mode(false) == DownloadOutputMode::Compact);

    unset_env_var("CI");
}

TEST_CASE("try_get_file_and_folder validates malformed url shapes", "[download][utils]")
{
    std::filesystem::path filepath;
    std::filesystem::path folder;
    auto const output_root = std::filesystem::path("data") / "gii";

    REQUIRE_FALSE(
        try_get_file_and_folder("https://www.gesetze-im-internet.de/onlyfolder/", output_root, filepath, folder));
    REQUIRE_FALSE(
        try_get_file_and_folder("https://www.gesetze-im-internet.de/x/not-xml.txt", output_root, filepath, folder));
    REQUIRE_FALSE(
        try_get_file_and_folder("https://www.gesetze-im-internet.de//xml.zip", output_root, filepath, folder));
}

TEST_CASE("setup_curl_handles configures valid urls and rejects invalid", "[download][utils]")
{
    auto const temp_root = std::filesystem::temp_directory_path() / "gdl_download_setup";
    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(temp_root);

    CurrentPathGuard const guard(temp_root);

    std::vector<std::string> urls{"bad-url", "https://www.gesetze-im-internet.de/test-law/xml.zip"};
    std::vector<CURL*> handles(urls.size());
    std::vector<std::unique_ptr<FILE, FileCloser>> files(urls.size());
    std::vector<bool> scheduled(urls.size());
    int setup_failures = 0;

    setup_curl_handles(urls, handles, files, scheduled, setup_failures, false, "data/gii");

    REQUIRE(setup_failures == 1);
    REQUIRE(handles.at(0) == nullptr);
    REQUIRE_FALSE(scheduled.at(0));

    REQUIRE(handles.at(1) != nullptr);
    REQUIRE(files.at(1) != nullptr);
    REQUIRE(scheduled.at(1));
    REQUIRE(std::filesystem::exists(temp_root / "data" / "gii" / "test-law"));
    REQUIRE(std::filesystem::exists(temp_root / "data" / "gii" / "test-law" / "xml.zip"));

    for (std::size_t i = 0; i < handles.size(); ++i) {
        files.at(i).reset();
        if (handles.at(i) != nullptr) {
            private_data const * data = nullptr;
            curl_easy_getinfo(handles.at(i), CURLINFO_PRIVATE, &data);
            // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
            delete data;
            curl_easy_setopt(handles.at(i), CURLOPT_PRIVATE, nullptr);
            curl_easy_cleanup(handles.at(i));
            handles.at(i) = nullptr;
        }
    }

    std::filesystem::remove_all(temp_root);
}

TEST_CASE("setup_curl_handles fails when target path is not writable file", "[download][utils]")
{
    auto const temp_root = std::filesystem::temp_directory_path() / "gdl_download_setup_file_open_fail";
    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(temp_root / "data" / "gii" / "blockedlaw" / "xml.zip");

    CurrentPathGuard const guard(temp_root);

    std::vector<std::string> urls{"https://www.gesetze-im-internet.de/blockedlaw/xml.zip"};
    std::vector<CURL*> handles(urls.size());
    std::vector<std::unique_ptr<FILE, FileCloser>> files(urls.size());
    std::vector<bool> scheduled(urls.size());
    int setup_failures = 0;

    setup_curl_handles(urls, handles, files, scheduled, setup_failures, false, "data/gii");

    REQUIRE(setup_failures == 1);
    REQUIRE(handles.at(0) == nullptr);
    REQUIRE(files.at(0) == nullptr);
    REQUIRE_FALSE(scheduled.at(0));

    std::filesystem::remove_all(temp_root);
}

TEST_CASE("setup_curl_multi_handle accepts null and active handles", "[download][utils]")
{
    CURL* easy = curl_easy_init();
    REQUIRE(easy != nullptr);

    std::vector<CURL*> const handles{nullptr, easy};
    CURLM* multi = setup_curl_multi_handle(handles);
    REQUIRE(multi != nullptr);

    std::vector<bool> const done{false, false};
    auto const active = collect_active_files(handles, done);
    REQUIRE(active.empty());

    curl_multi_cleanup(multi);
    curl_easy_cleanup(easy);
}

TEST_CASE("download_file returns failure when target cannot be opened", "[download][utils]")
{
    auto const temp_root = std::filesystem::temp_directory_path() / "gdl_download_file_open";
    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(temp_root);

    auto const impossible_path = temp_root / "missing" / "out.zip";
    REQUIRE(download_file("https://example.com", impossible_path.string()) == EXIT_FAILURE);

    std::filesystem::remove_all(temp_root);
}

TEST_CASE("download_file handles transfer failure scenarios", "[download][utils]")
{
    auto const temp_root = std::filesystem::temp_directory_path() / "gdl_download_file_transfer";
    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(temp_root);

    auto const failed_out = temp_root / "failed.bin";
    REQUIRE(download_file("http://127.0.0.1:1/", failed_out.string()) == EXIT_FAILURE);
    REQUIRE_FALSE(std::filesystem::exists(failed_out));

    auto const source     = temp_root / "source.txt";
    auto const second_out = temp_root / "second.bin";
    {
        std::ofstream out(source.string());
        out << "hello";
    }
    auto const file_url = std::string("file://") + source.string();
    REQUIRE(download_file(file_url, second_out.string()) == EXIT_FAILURE);
    REQUIRE_FALSE(std::filesystem::exists(second_out));

    std::filesystem::remove_all(temp_root);
}

TEST_CASE("download_file succeeds against local HTTP fixture", "[download][utils]")
{
#ifdef _WIN32
    SUCCEED("Local POSIX socket fixture is not enabled on Windows.");
#else
    auto const temp_root = std::filesystem::temp_directory_path() / "gdl_download_file_success";
    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(temp_root);

    std::string const expected = "fixture-download-payload";
    LocalHttpFixture const fixture(expected);

    auto const output = temp_root / "downloaded.bin";
    REQUIRE(download_file(fixture.url(), output.string()) == EXIT_SUCCESS);
    REQUIRE(std::filesystem::exists(output));

    std::ifstream in(output.string(), std::ios::binary);
    std::string const content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    REQUIRE(content == expected);

    std::filesystem::remove_all(temp_root);
#endif
}

TEST_CASE("download status renderers execute interactive and compact paths", "[download][utils]")
{
    TestDownloadProgressState state;
    state.total       = 10;
    state.finished    = 2;
    state.succeeded   = 1;
    state.failed      = 1;
    state.output_mode = DownloadOutputMode::Interactive;

    render_interactive_status(state, {"a.xml", "b.xml"});

    state.interactive_rendered = true;
    render_interactive_status(state, {"a.xml"});

    state.output_mode         = DownloadOutputMode::Compact;
    state.started_at          = std::chrono::steady_clock::now() - std::chrono::seconds(65);
    state.last_compact_log_at = std::chrono::steady_clock::now() - std::chrono::seconds(31);
    maybe_render_compact_status(state, {"active.xml"}, false);

    state.finished                  = state.total;
    state.last_compact_log_finished = state.finished;
    maybe_render_compact_status(state, {}, false);

    state.output_mode          = DownloadOutputMode::Interactive;
    state.interactive_rendered = false;
    render_download_status(state, {"active.xml"}, false);
    REQUIRE(state.interactive_rendered);

    render_download_status(state, {}, true);
}

TEST_CASE("download_urls handles chunked invalid inputs", "[download][utils]")
{
    std::vector<std::string> urls;
    urls.reserve(10);
    for (int i = 0; i < 10; ++i) {
        urls.emplace_back("invalid-url");
    }

    REQUIRE(download_urls(urls, false, "data/gii") == EXIT_SUCCESS);
}

TEST_CASE("download_urls executes transfer loop for valid-shaped URL", "[download][utils]")
{
    auto const temp_root = std::filesystem::temp_directory_path() / "gdl_download_loop";
    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(temp_root);

    CurrentPathGuard const guard(temp_root);

    std::vector<std::string> const urls{
        "https://www.gesetze-im-internet.de/definitely-not-existing-gdl-test/"
        "xml.zip"};

    REQUIRE(download_urls(urls, false, "data/gii") == EXIT_SUCCESS);

    std::filesystem::remove_all(temp_root);
}
