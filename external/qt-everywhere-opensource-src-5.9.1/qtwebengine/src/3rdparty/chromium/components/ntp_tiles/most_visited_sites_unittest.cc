// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/most_visited_sites.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_tiles {

namespace {

struct TitleURL {
  TitleURL(const std::string& title, const std::string& url)
      : title(base::UTF8ToUTF16(title)), url(url) {}
  TitleURL(const base::string16& title, const std::string& url)
      : title(title), url(url) {}

  base::string16 title;
  std::string url;

  bool operator==(const TitleURL& other) const {
    return title == other.title && url == other.url;
  }
};

static const size_t kNumSites = 4;

}  // namespace

// This a test for MostVisitedSites::MergeTiles(...) method, and thus has the
// same scope as the method itself. This tests merging popular sites with
// personal tiles.
// More important things out of the scope of testing presently:
// - Removing blacklisted tiles.
// - Correct host extraction from the URL.
// - Ensuring personal tiles are not duplicated in popular tiles.
class MostVisitedSitesTest : public testing::Test {
 protected:
  void Check(const std::vector<TitleURL>& popular_sites,
             const std::vector<TitleURL>& whitelist_entry_points,
             const std::vector<TitleURL>& personal_sites,
             const std::vector<bool>& expected_sites_is_personal,
             const std::vector<TitleURL>& expected_sites) {
    NTPTilesVector personal_tiles;
    for (const TitleURL& site : personal_sites)
      personal_tiles.push_back(MakeTileFrom(site, true, false));
    NTPTilesVector whitelist_tiles;
    for (const TitleURL& site : whitelist_entry_points)
      whitelist_tiles.push_back(MakeTileFrom(site, false, true));
    NTPTilesVector popular_tiles;
    for (const TitleURL& site : popular_sites)
      popular_tiles.push_back(MakeTileFrom(site, false, false));
    NTPTilesVector result_tiles = MostVisitedSites::MergeTiles(
        std::move(personal_tiles), std::move(whitelist_tiles),
        std::move(popular_tiles));
    std::vector<TitleURL> result_sites;
    std::vector<bool> result_is_personal;
    result_sites.reserve(result_tiles.size());
    result_is_personal.reserve(result_tiles.size());
    for (const auto& tile : result_tiles) {
      result_sites.push_back(TitleURL(tile.title, tile.url.spec()));
      result_is_personal.push_back(tile.source != NTPTileSource::POPULAR);
    }
    EXPECT_EQ(expected_sites_is_personal, result_is_personal);
    EXPECT_EQ(expected_sites, result_sites);
  }
  static NTPTile MakeTileFrom(const TitleURL& title_url,
                              bool is_personal,
                              bool whitelist) {
    NTPTile tile;
    tile.title = title_url.title;
    tile.url = GURL(title_url.url);
    tile.source = whitelist ? NTPTileSource::WHITELIST
                            : (is_personal ? NTPTileSource::TOP_SITES
                                           : NTPTileSource::POPULAR);
    return tile;
  }
};

TEST_F(MostVisitedSitesTest, PersonalSites) {
  std::vector<TitleURL> personal_sites{
      TitleURL("Site 1", "https://www.site1.com/"),
      TitleURL("Site 2", "https://www.site2.com/"),
      TitleURL("Site 3", "https://www.site3.com/"),
      TitleURL("Site 4", "https://www.site4.com/"),
  };
  // Without any popular tiles, the result after merge should be the personal
  // tiles.
  std::vector<bool> expected_sites_source(kNumSites, true /*personal source*/);
  Check(std::vector<TitleURL>(), std::vector<TitleURL>(), personal_sites,
        expected_sites_source, personal_sites);
}

TEST_F(MostVisitedSitesTest, PopularSites) {
  std::vector<TitleURL> popular_sites{
      TitleURL("Site 1", "https://www.site1.com/"),
      TitleURL("Site 2", "https://www.site2.com/"),
      TitleURL("Site 3", "https://www.site3.com/"),
      TitleURL("Site 4", "https://www.site4.com/"),
  };
  // Without any personal tiles, the result after merge should be the popular
  // tiles.
  std::vector<bool> expected_sites_source(kNumSites, false /*popular source*/);
  Check(popular_sites, std::vector<TitleURL>(), std::vector<TitleURL>(),
        expected_sites_source, popular_sites);
}

TEST_F(MostVisitedSitesTest, PersonalPrecedePopularSites) {
  std::vector<TitleURL> popular_sites{
      TitleURL("Site 1", "https://www.site1.com/"),
      TitleURL("Site 2", "https://www.site2.com/"),
  };
  std::vector<TitleURL> personal_sites{
      TitleURL("Site 3", "https://www.site3.com/"),
      TitleURL("Site 4", "https://www.site4.com/"),
  };
  // Personal tiles should precede popular tiles.
  std::vector<TitleURL> expected_sites{
      TitleURL("Site 3", "https://www.site3.com/"),
      TitleURL("Site 4", "https://www.site4.com/"),
      TitleURL("Site 1", "https://www.site1.com/"),
      TitleURL("Site 2", "https://www.site2.com/"),
  };
  std::vector<bool> expected_sites_source{true, true, false, false};
  Check(popular_sites, std::vector<TitleURL>(), personal_sites,
        expected_sites_source, expected_sites);
}

}  // namespace ntp_tiles
