// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

#include "utils/bgbl_og.h"

namespace
{
    // Repräsentative Antwort der OffeneGesetze.de-API. Zwei »orders« der Ausgabe
    // Nr. 40 (unterschiedliche Gesetze) teilen sich dasselbe konsolidierte PDF; ein
    // dritter order gehört zur Ausgabe Nr. 41.
    constexpr char const * API_JSON = R"({
    "count": 3,
    "next": null,
    "previous": null,
    "results": [
        {
            "id": "bgbl1-2018-40-1",
            "kind": "bgbl1",
            "year": 2018,
            "number": 40,
            "date": "2018-12-04T00:00:00Z",
            "url": "https://offenegesetze.de/veroeffentlichung/bgbl1/2018/40#page=2",
            "api_url": "https://api.offenegesetze.de/v1/veroeffentlichung/bgbl1-2018-40-1/",
            "document_url": "https://media.offenegesetze.de/bgbl1/2018/bgbl1_2018_40.pdf#page=2",
            "order": 1,
            "title": "Gesetz zur Änderung des Zensusvorbereitungsgesetzes"
        },
        {
            "id": "bgbl1-2018-40-2",
            "kind": "bgbl1",
            "year": 2018,
            "number": 40,
            "date": "2018-12-04T00:00:00Z",
            "url": "https://offenegesetze.de/veroeffentlichung/bgbl1/2018/40#page=4",
            "api_url": "https://api.offenegesetze.de/v1/veroeffentlichung/bgbl1-2018-40-2/",
            "document_url": "https://media.offenegesetze.de/bgbl1/2018/bgbl1_2018_40.pdf#page=4",
            "order": 2,
            "title": "Zweites Gesetz zur Änderung des Seearbeitsgesetzes"
        },
        {
            "id": "bgbl1-2018-41-1",
            "kind": "bgbl1",
            "year": 2018,
            "number": 41,
            "date": "2018-12-12T00:00:00Z",
            "url": "https://offenegesetze.de/veroeffentlichung/bgbl1/2018/41#page=2",
            "api_url": "https://api.offenegesetze.de/v1/veroeffentlichung/bgbl1-2018-41-1/",
            "document_url": "https://media.offenegesetze.de/bgbl1/2018/bgbl1_2018_41.pdf#page=2",
            "order": 1,
            "title": "Gesetz über den Nachtragshaushaltsplan"
        }
    ],
    "facets": {
        "kind": [],
        "year": [
            { "value": 2018, "count": 3, "selected": false }
        ]
    }
})";

    constexpr char const * EMPTY_JSON = R"({
    "count": 0,
    "next": null,
    "previous": null,
    "results": [],
    "facets": { "kind": [], "year": [] }
})";
} // namespace

TEST_CASE("bgbl::og api_url builds part/year/number query", "[bgbl][og][utils]")
{
    REQUIRE(
        bgbl::og::api_url(1, 2018, 40) ==
        "https://api.offenegesetze.de/v1/veroeffentlichung/?year=2018&kind=bgbl1&number=40");
    REQUIRE(bgbl::og::api_url(2, 2018) == "https://api.offenegesetze.de/v1/veroeffentlichung/?year=2018&kind=bgbl2");
    REQUIRE(
        bgbl::og::api_url(1, 2019, std::nullopt) ==
        "https://api.offenegesetze.de/v1/veroeffentlichung/?year=2019&kind=bgbl1");
}

TEST_CASE("bgbl::og parse_entries de-duplicates by document_url", "[bgbl][og][utils]")
{
    auto const entries = bgbl::og::parse_entries(API_JSON);
    REQUIRE(entries.size() == 2); // Nr. 40 + Nr. 41, die beiden Nr.-40-Gesetze teilen sich ein PDF
    REQUIRE(entries.at(0).number == 40);
    REQUIRE(entries.at(0).document_url == "https://media.offenegesetze.de/bgbl1/2018/bgbl1_2018_40.pdf");
    REQUIRE(entries.at(0).date == "2018-12-04T00:00:00Z");
    REQUIRE(entries.at(1).number == 41);
    REQUIRE(entries.at(1).document_url == "https://media.offenegesetze.de/bgbl1/2018/bgbl1_2018_41.pdf");
}

TEST_CASE("bgbl::og parse_issues de-duplicates by issue number", "[bgbl][og][utils]")
{
    auto const issues = bgbl::og::parse_issues(API_JSON);
    REQUIRE(issues.size() == 2);
    REQUIRE(issues.at(0).number == 40);
    REQUIRE(issues.at(1).number == 41);
}

TEST_CASE("bgbl::og parsing handles an empty result list", "[bgbl][og][utils]")
{
    REQUIRE(bgbl::og::parse_entries(EMPTY_JSON).empty());
    REQUIRE(bgbl::og::parse_issues(EMPTY_JSON).empty());
}
