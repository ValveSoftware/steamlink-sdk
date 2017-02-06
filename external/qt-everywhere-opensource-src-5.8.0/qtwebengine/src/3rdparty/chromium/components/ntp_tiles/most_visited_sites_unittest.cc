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

// This a test for MostVisitedSites::MergeSuggestions(...) method, and thus has
// the same scope as the method itself. This tests merging popular suggestions
// with personal suggestions.
// More important things out of the scope of testing presently:
// - Removing blacklisted suggestions.
// - Correct host extraction from the URL.
// - Ensuring personal suggestions are not duplicated in popular suggestions.
class MostVisitedSitesTest : public testing::Test {
 protected:
  void Check(const std::vector<TitleURL>& popular_sites,
             const std::vector<TitleURL>& whitelist_entry_points,
             const std::vector<TitleURL>& personal_sites,
             const std::vector<bool>& expected_sites_is_personal,
             const std::vector<TitleURL>& expected_sites) {
    MostVisitedSites::SuggestionsVector personal_suggestions;
    for (const TitleURL& site : personal_sites)
      personal_suggestions.push_back(MakeSuggestionFrom(site, true, false));
    MostVisitedSites::SuggestionsVector whitelist_suggestions;
    for (const TitleURL& site : whitelist_entry_points)
      whitelist_suggestions.push_back(MakeSuggestionFrom(site, false, true));
    MostVisitedSites::SuggestionsVector popular_suggestions;
    for (const TitleURL& site : popular_sites)
      popular_suggestions.push_back(MakeSuggestionFrom(site, false, false));
    MostVisitedSites::SuggestionsVector result_suggestions =
        MostVisitedSites::MergeSuggestions(std::move(personal_suggestions),
                                           std::move(whitelist_suggestions),
                                           std::move(popular_suggestions));
    std::vector<TitleURL> result_sites;
    std::vector<bool> result_is_personal;
    result_sites.reserve(result_suggestions.size());
    result_is_personal.reserve(result_suggestions.size());
    for (const auto& suggestion : result_suggestions) {
      result_sites.push_back(TitleURL(suggestion.title, suggestion.url.spec()));
      result_is_personal.push_back(suggestion.source !=
                                   MostVisitedSites::POPULAR);
    }
    EXPECT_EQ(expected_sites_is_personal, result_is_personal);
    EXPECT_EQ(expected_sites, result_sites);
  }
  static MostVisitedSites::Suggestion MakeSuggestionFrom(
      const TitleURL& title_url,
      bool is_personal,
      bool whitelist) {
    MostVisitedSites::Suggestion suggestion;
    suggestion.title = title_url.title;
    suggestion.url = GURL(title_url.url);
    suggestion.source = whitelist ? MostVisitedSites::WHITELIST
                                  : (is_personal ? MostVisitedSites::TOP_SITES
                                                 : MostVisitedSites::POPULAR);
    return suggestion;
  }
};

TEST_F(MostVisitedSitesTest, PersonalSites) {
  std::vector<TitleURL> personal_sites{
      TitleURL("Site 1", "https://www.site1.com/"),
      TitleURL("Site 2", "https://www.site2.com/"),
      TitleURL("Site 3", "https://www.site3.com/"),
      TitleURL("Site 4", "https://www.site4.com/"),
  };
  // Without any popular suggestions, the result after merge should be the
  // personal suggestions.
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
  // Without any personal suggestions, the result after merge should be the
  // popular suggestions.
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
  // Personal suggestions should precede popular suggestions.
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
