// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "utils/bgbl_ris.h"

namespace
{
    // Repräsentative RIS- (Hydra-)Sammlungsseite. Zwei Gesetze des BGBl I 2026
    // (eine reguläre Ausgabe 201, eine Ergänzungsausgabe »67-2«) und eine BGBl-II-2026-
    // Ausgabe, die bei Teil == 1 herausgefiltert werden muss.
    constexpr char const * PAGE_JSON = R"({
    "totalItems": 3,
    "member": [
        { "item": { "@type":"Legislation", "@id":"/v1/legislation/eli/bund/bgbl-1/2026/201/x", "name":"Verordnung zur Durchführung des Treibhausgas-Emissionshandelsgesetzes", "exampleOfWork": { "legislationIdentifier":"eli/bund/bgbl-1/2026/201", "legislationDate":"2026-07-10", "datePublished":"2026-07-14", "isPartOf": { "@type":"PublicationIssue", "name":"BGBl I, 2026 201" } } }, "textMatches":[] },
        { "item": { "@type":"Legislation", "@id":"/v1/legislation/eli/bund/bgbl-1/2026/67-2/x", "name":"Gesetz über eine Sonderausgabe", "exampleOfWork": { "legislationIdentifier":"eli/bund/bgbl-1/2026/67-2", "legislationDate":"2026-03-01", "datePublished":"2026-03-05", "isPartOf": { "@type":"PublicationIssue", "name":"BGBl I, 2026 67-2" } } }, "textMatches":[] },
        { "item": { "@type":"Legislation", "@id":"/v1/legislation/eli/bund/bgbl-2/2026/10/x", "name":"Zweites Gesetz zur Änderung", "exampleOfWork": { "isPartOf": { "@type":"PublicationIssue", "name":"BGBl II, 2026 10" } } }, "textMatches":[] }
    ],
    "view": { "@type":"hydra:PartialCollectionView", "first":"/v1/legislation?pageIndex=0&size=200", "previous":null, "next":"/v1/legislation?pageIndex=1&size=200", "last":"/v1/legislation?pageIndex=2&size=200" }
})";

    constexpr char const * LAST_PAGE_JSON = R"({
    "totalItems": 3,
    "member": [],
    "view": { "@type":"hydra:PartialCollectionView", "first":"/v1/legislation?pageIndex=0&size=200", "previous":"/v1/legislation?pageIndex=0&size=200", "next":null, "last":"/v1/legislation?pageIndex=2&size=200" }
})";
} // namespace

TEST_CASE("bgbl::ris pdf_url builds the deterministic ELI PDF URL", "[bgbl][ris][utils]")
{
    REQUIRE(
        bgbl::ris::pdf_url(1, 2026, "201") ==
        "https://www.recht.bund.de/eli/bund/BGBl-1/2026/201/regelungstext.pdf?__blob=publicationFile");
    REQUIRE(
        bgbl::ris::pdf_url(2, 2026, "67-2") ==
        "https://www.recht.bund.de/eli/bund/BGBl-2/2026/67-2/regelungstext.pdf?__blob=publicationFile");
}

TEST_CASE("bgbl::ris parse_page extracts issues and filters by part", "[bgbl][ris][utils]")
{
    auto const entries = bgbl::ris::parse_page(PAGE_JSON, 1);
    REQUIRE(entries.size() == 2);
    REQUIRE(entries.at(0).year == 2026);
    REQUIRE(entries.at(0).number == "201");
    REQUIRE(entries.at(0).date == "2026-07-14");
    REQUIRE(entries.at(0).title == "Verordnung zur Durchführung des Treibhausgas-Emissionshandelsgesetzes");
    REQUIRE(entries.at(1).number == "67-2");
    REQUIRE(entries.at(1).date == "2026-03-05");
}

TEST_CASE("bgbl::ris parse_page keeps only the requested part", "[bgbl][ris][utils]")
{
    auto const entries = bgbl::ris::parse_page(PAGE_JSON, 2);
    REQUIRE(entries.size() == 1);
    REQUIRE(entries.at(0).number == "10");
}

TEST_CASE("bgbl::ris next_page extracts the relative next URL", "[bgbl][ris][utils]")
{
    REQUIRE(bgbl::ris::next_page(PAGE_JSON) == "/v1/legislation?pageIndex=1&size=200");
    REQUIRE(bgbl::ris::next_page(LAST_PAGE_JSON).empty());
}
