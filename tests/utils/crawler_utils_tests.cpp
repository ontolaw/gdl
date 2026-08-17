// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "utils/crawler.h"

namespace
{
#ifndef _WIN32
    class LocalCrawlerFixture
    {
    public:
        LocalCrawlerFixture() : server_fd_(::socket(AF_INET, SOCK_STREAM, 0))
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

            if (::listen(server_fd_, 8) < 0) {
                ::close(server_fd_);
                server_fd_ = -1;
                throw std::runtime_error("listen() failed");
            }

            server_thread_ = std::thread([this]() {
                for (int i = 0; i < 6; ++i) {
                    sockaddr_in client_addr{};
                    socklen_t client_len = sizeof(client_addr);
                    int const client_fd  = ::accept(
                        server_fd_,
                        // NOLINTNEXTLINE(bugprone-casting-through-void)
                        static_cast<sockaddr*>(static_cast<void*>(&client_addr)),
                        &client_len);
                    if (client_fd < 0) {
                        continue;
                    }

                    std::array<char, 1024> request_buffer{};
                    ssize_t const received = ::recv(client_fd, request_buffer.data(), request_buffer.size(), 0);
                    if (received <= 0) {
                        ::close(client_fd);
                        continue;
                    }
                    request_buffer.at(received) = '\0';

                    std::string const request(request_buffer.data());
                    std::string body;
                    std::string content_type = "text/plain";

                    if (request.contains("GET /page.htm ")) {
                        content_type = "text/html";
                        body         = R"(<html><body><a href="/pdf/sample.pdf">PDF</a></body></html>)";
                    } else if (request.contains("GET /pdf/sample.pdf ")) {
                        content_type = "application/pdf";
                        body         = "%PDF-1.4\n%fixture\n";
                    } else {
                        body = "not found";
                    }

                    std::string const status = (body == "not found") ? "404 Not Found" : "200 OK";
                    std::string response     = "HTTP/1.1 ";
                    response += status;
                    response += "\r\nContent-Type: ";
                    response += content_type;
                    response += "\r\nContent-Length: ";
                    response += std::to_string(body.size());
                    response += "\r\nConnection: close\r\n\r\n";
                    response += body;

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
                }
            });
        }

        LocalCrawlerFixture(LocalCrawlerFixture const &)            = delete;
        LocalCrawlerFixture& operator=(LocalCrawlerFixture const &) = delete;
        LocalCrawlerFixture(LocalCrawlerFixture&&)                  = delete;
        LocalCrawlerFixture& operator=(LocalCrawlerFixture&&)       = delete;

        ~LocalCrawlerFixture()
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

        [[nodiscard]] std::string page_url() const
        { return "http://127.0.0.1:" + std::to_string(port_) + "/page.htm"; }

    private:
        int server_fd_ = -1;
        uint16_t port_ = 0;
        std::thread server_thread_;
    };
#endif
} // namespace

TEST_CASE("crawl_vvii_from_toc downloads linked PDF artifact", "[crawler][utils]")
{
#ifdef _WIN32
    SUCCEED("crawler fixture test is currently implemented for POSIX only");
#else
    LocalCrawlerFixture const fixture;

    auto const temp_root = std::filesystem::temp_directory_path() / "gdl_crawler_utils";
    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(temp_root);

    auto const toc_file = temp_root / "vvii-toc.xml";
    {
        std::ofstream out(toc_file.string());
        out << "<items><item><link>" << fixture.page_url() << "</link></item></items>";
    }

    REQUIRE(crawl_vvii_from_toc(toc_file, temp_root, false, std::nullopt) == EXIT_SUCCESS);

    REQUIRE(std::filesystem::exists(temp_root / "raw-pages" / "127.0.0.1"));
    REQUIRE(std::filesystem::exists(temp_root / "raw-artifacts" / "127.0.0.1" / "pdf" / "sample.pdf"));

    REQUIRE(std::filesystem::exists(temp_root / "state" / "frontier-pages.txt"));
    REQUIRE(std::filesystem::exists(temp_root / "state" / "visited-pages.txt"));
    REQUIRE(std::filesystem::exists(temp_root / "state" / "discovered-artifacts.txt"));
    REQUIRE(std::filesystem::exists(temp_root / "state" / "downloaded-artifacts.txt"));

    std::filesystem::remove_all(temp_root);
#endif
}
