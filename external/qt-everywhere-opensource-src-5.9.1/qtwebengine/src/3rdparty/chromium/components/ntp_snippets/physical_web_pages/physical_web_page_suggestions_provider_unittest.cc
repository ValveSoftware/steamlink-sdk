// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/physical_web_pages/physical_web_page_suggestions_provider.h"

#include <string>
#include <vector>

#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/ntp_snippets/mock_content_suggestions_provider_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;

namespace ntp_snippets {

namespace {

UrlInfo CreateUrlInfo(const std::string& site_url) {
  UrlInfo url_info;
  url_info.site_url = GURL(site_url);
  return url_info;
}

std::vector<UrlInfo> CreateUrlInfos(const std::vector<std::string>& site_urls) {
  std::vector<UrlInfo> url_infos;
  for (const std::string& site_url : site_urls) {
    url_infos.emplace_back(CreateUrlInfo(site_url));
  }
  return url_infos;
}

MATCHER_P(HasUrl, url, "") {
  *result_listener << "expected URL: " << url
                   << "has URL: " << arg.url().spec();
  return arg.url().spec() == url;
}

}  // namespace

TEST(PhysicalWebPageSuggestionsProviderTest, ShouldCreateSuggestions) {
  MockContentSuggestionsProviderObserver observer;
  CategoryFactory category_factory;
  Category category =
      category_factory.FromKnownCategory(KnownCategories::PHYSICAL_WEB_PAGES);
  PhysicalWebPageSuggestionsProvider provider(&observer, &category_factory);
  const std::string first_url = "http://test1.com/";
  const std::string second_url = "http://test2.com/";
  const std::vector<UrlInfo> url_infos =
      CreateUrlInfos({first_url, second_url});

  EXPECT_CALL(observer,
              OnNewSuggestions(
                  &provider, category,
                  UnorderedElementsAre(HasUrl(first_url), HasUrl(second_url))));
  provider.OnDisplayableUrlsChanged(url_infos);
}

}  // namespace ntp_snippets
