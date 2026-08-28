// Copyright 2021-2026 Jens A. Koch.
// SPDX-License-Identifier: MIT
// This file is part of ontolaw/gdl.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "utils/bgbl.h"

namespace
{
    // Erfasste / repräsentative XAVER-Antwortformen (kompaktes JSON, ohne Leerzeichen).
    constexpr char const * ROOT_JSON =
        R"({"identifier":"id","label":"l","items":[{"id":"61929556","did":"1223850","l":"Bundesgesetzblatt","mrk":0,"c":[{"id":"61929557","did":"1223851","l":"Bundesgesetzblatt Teil I","mrk":0,"t":"s","c":true},{"id":"61980564","did":"1274783","l":"Bundesgesetzblatt Teil II","mrk":0,"t":"s","c":true}]}]})";

    constexpr char const * YEAR_JSON =
        R"({"identifier":"id","label":"l","items":[{"id":"61980524","did":"1274744","l":"1949","mrk":0,"c":[{"id":"61980525","did":"1274744","l":"Nr. 9 vom 31.12.1949","mrk":0,"t":"s","c":true},{"id":"61980560","did":"1274779","l":"Nr. 1 vom 23.05.1949","mrk":0,"t":"s","c":true}]}]})";

    constexpr char const * ISSUE_HTML =
        R"(<a href="text.xav?SID=&amp;skin=&amp;start=%2F%2F*%5B%40node_id%3D%271274745%27%5D&amp;origin=link&amp;hlf=xaver.component.Hitlist_0&amp;tf=xaver.component.Text_0#bgbl149009.pdf" onclick="return xaver.globals.handleLink(this);">Komplette Ausgabe</a>)";

    constexpr char const * MEDIA_HTML =
        R"(<img border="0" src="media.xav/bgbl149009.pdf?SID=&amp;iid=2662&amp;sinst=&amp;ssinst=&amp;_csrf=ABCDEF" /></span>)";
} // namespace

TEST_CASE("bgbl part_label maps part to node label", "[bgbl][utils]")
{
    REQUIRE(bgbl::part_label(1) == "Bundesgesetzblatt Teil I");
    REQUIRE(bgbl::part_label(2) == "Bundesgesetzblatt Teil II");
}

TEST_CASE("bgbl parse_part accepts numeric and roman forms", "[bgbl][utils]")
{
    int part = 0;
    REQUIRE(bgbl::parse_part("1", part));
    REQUIRE(part == 1);
    REQUIRE(bgbl::parse_part("I", part));
    REQUIRE(part == 1);
    REQUIRE(bgbl::parse_part("ii", part));
    REQUIRE(part == 2);
    REQUIRE(bgbl::parse_part("nope", part) == false);
}

TEST_CASE("bgbl find_item_id locates Teil I node", "[bgbl][utils]")
{
    REQUIRE(bgbl::find_item_id(ROOT_JSON, "Bundesgesetzblatt Teil I") == "61929557");
    // Präfix-Treffer findet den Knoten weiterhin
    REQUIRE(bgbl::find_item_id(ROOT_JSON, "Bundesgesetzblatt Teil", false) == "61929557");
    // exakte Übereinstimmung darf einen kürzeren Präfix nicht als vollständiges Label matchen
    REQUIRE(bgbl::find_item_id(ROOT_JSON, "Bundesgesetzblatt Teil", true).empty());
    REQUIRE(bgbl::find_item_id(ROOT_JSON, "nope").empty());
}

TEST_CASE("bgbl find_item_id locates the year node", "[bgbl][utils]")
{ REQUIRE(bgbl::find_item_id(YEAR_JSON, "1949") == "61980524"); }

TEST_CASE("bgbl find_item_did locates an issue by prefix", "[bgbl][utils]")
{
    REQUIRE(bgbl::find_item_did(YEAR_JSON, "Nr. 1 vom", false) == "1274779");
    REQUIRE(bgbl::find_item_did(YEAR_JSON, "Nr. 9 vom", false) == "1274744");
    REQUIRE(bgbl::find_item_did(YEAR_JSON, "Nr. 99 vom").empty());
}

TEST_CASE("bgbl find_komplette_node finds the node before 'Komplette Ausgabe'", "[bgbl][utils]")
{
    REQUIRE(bgbl::find_komplette_node(ISSUE_HTML) == "1274745");
    REQUIRE(bgbl::find_komplette_node("no komplette link here").empty());
}

TEST_CASE("bgbl extract_media_url resolves the absolute download URL", "[bgbl][utils]")
{
    std::string const url = bgbl::extract_media_url(MEDIA_HTML);
    REQUIRE(url == "https://www.bgbl.de/xaver/bgbl/media.xav/bgbl149009.pdf?SID=&iid=2662&sinst=&ssinst=&_csrf=ABCDEF");
    REQUIRE(bgbl::extract_media_url("no media here").empty());
}

TEST_CASE("bgbl list_issues returns all issues with numbers", "[bgbl][utils]")
{
    auto const issues = bgbl::list_issues(YEAR_JSON);
    REQUIRE(issues.size() == 2);
    REQUIRE(issues.at(0).nr == 9);
    REQUIRE(issues.at(0).did == "1274744");
    REQUIRE(issues.at(0).id == "61980525");
    REQUIRE(issues.at(1).nr == 1);
    REQUIRE(issues.at(1).did == "1274779");
    REQUIRE(issues.at(1).id == "61980560");
}

TEST_CASE("bgbl list_issue_entries returns labels alongside numbers", "[bgbl][utils]")
{
    auto const entries = bgbl::list_issue_entries(YEAR_JSON);
    REQUIRE(entries.size() == 2);
    REQUIRE(entries.at(0).nr == 9);
    REQUIRE(entries.at(0).did == "1274744");
    REQUIRE(entries.at(0).label == "Nr. 9 vom 31.12.1949");
    REQUIRE(entries.at(1).nr == 1);
    REQUIRE(entries.at(1).did == "1274779");
    REQUIRE(entries.at(1).label == "Nr. 1 vom 23.05.1949");
}
