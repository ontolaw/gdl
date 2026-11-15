// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

#include <catch2/catch_test_macros.hpp>

#include "test_helpers/temp_dir.h"
#include "utils/xml.h"

void rename_xml_nodes(pugi::xml_document& doc, std::unordered_map<std::string, std::string> const & node_map);

TEST_CASE("get_urls extracts link entries from toc XML", "[xml]")
{
    auto const temp_dir = std::filesystem::temp_directory_path() / "gdl_xml_utils_test";
    auto const toc_file = temp_dir / "gii-toc.xml";

    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    {
        std::ofstream out(toc_file.string());
        out << "<items>"
            << "<item><link>https://www.gesetze-im-internet.de/a/xml.zip</link></"
               "item>"
            << "<item><name>ignore</name><link>https://www.gesetze-im-internet.de/"
               "b/xml.zip</link></item>"
            << "<item><name>without-link</name></item>"
            << "</items>";
    }

    auto const urls = get_urls(toc_file.string());
    REQUIRE(urls.size() == 2);
    REQUIRE(urls.at(0) == "https://www.gesetze-im-internet.de/a/xml.zip");
    REQUIRE(urls.at(1) == "https://www.gesetze-im-internet.de/b/xml.zip");

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("get_urls returns empty for missing or invalid xml", "[xml]")
{
    REQUIRE(get_urls("/definitely/not/found/gii-toc.xml").empty());

    auto const temp_no_items_dir = std::filesystem::temp_directory_path() / "gdl_xml_utils_test_no_items";
    auto const no_items_file     = temp_no_items_dir / "no-items.xml";
    std::filesystem::remove_all(temp_no_items_dir);
    std::filesystem::create_directories(temp_no_items_dir);
    {
        std::ofstream out(no_items_file.string());
        out << "<root><item><link>https://example.invalid/x.xml</link></item></"
               "root>";
    }
    REQUIRE(get_urls(no_items_file.string()).empty());
    std::filesystem::remove_all(temp_no_items_dir);

    auto const temp_dir = std::filesystem::temp_directory_path() / "gdl_xml_utils_test_invalid";
    auto const bad_file = temp_dir / "broken.xml";

    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);
    {
        std::ofstream out(bad_file.string());
        out << "<items><item><link>oops";
    }

    REQUIRE(get_urls(bad_file.string()).empty());

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("format_xml_files rewrites xml files in folder tree", "[xml]")
{
    test_helpers::TempDir const temp_dir_owner{"gdl_xml_format_test"};
    auto const temp_dir = temp_dir_owner.path();
    auto const nested   = temp_dir / "sub";
    auto const xml_file = nested / "sample.xml";

    std::filesystem::create_directories(nested);

    {
        std::ofstream out(xml_file.string());
        out << "<root><child>1</child></root>";
    }

    format_xml_files(temp_dir.string());

    REQUIRE(std::filesystem::exists(xml_file));
    std::ifstream in(xml_file.string());
    std::string const content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    REQUIRE(content.contains("<root>"));
    REQUIRE(content.contains("<child>1</child>"));

    format_xml_files((temp_dir / "missing").string());
}

TEST_CASE("format_xml_files is deterministic and produces byte-identical output on re-run", "[xml]")
{
    test_helpers::TempDir const temp_dir_owner{"gdl_xml_determinism_test"};
    auto const temp_dir = temp_dir_owner.path();
    auto const nested   = temp_dir / "sub";
    auto const xml_file = nested / "sample.xml";

    std::filesystem::create_directories(nested);

    {
        // Nicht eingerückte Eingabe mit Attributen, um stabile Re-Serialisierung zu prüfen.
        std::ofstream out(xml_file.string());
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
               "<root b=\"2\" a=\"1\"><child>text &amp; more</child></root>";
    }

    auto const read_bytes = [&](std::filesystem::path const & p) {
        std::ifstream in(p.string(), std::ios::binary);
        std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        return s;
    };

    format_xml_files(temp_dir.string());
    std::string const first = read_bytes(xml_file);

    format_xml_files(temp_dir.string());
    std::ifstream in2(xml_file.string(), std::ios::binary);
    std::string const second((std::istreambuf_iterator<char>(in2)), std::istreambuf_iterator<char>());

    REQUIRE_FALSE(first.empty());
    REQUIRE(first == second);
}

TEST_CASE("rename_xml_nodes renames existing nodes and skips missing ones", "[xml]")
{
    pugi::xml_document doc;
    REQUIRE(doc.load_string("<old>v</old>"));

    std::unordered_map<std::string, std::string> const mapping{{"old", "new"}, {"missing", "unused"}};

    rename_xml_nodes(doc, mapping);

    REQUIRE(doc.child("new"));
    REQUIRE_FALSE(doc.child("old"));
}
