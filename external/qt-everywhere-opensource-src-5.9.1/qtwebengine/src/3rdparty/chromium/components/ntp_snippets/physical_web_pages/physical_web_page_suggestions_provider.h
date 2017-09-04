// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_PHYSICAL_WEB_PAGES_PHYSICAL_WEB_PAGE_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_NTP_SNIPPETS_PHYSICAL_WEB_PAGES_PHYSICAL_WEB_PAGE_SUGGESTIONS_PROVIDER_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_provider.h"

namespace gfx {
class Image;
}  // namespace gfx

namespace ntp_snippets {

// TODO(vitaliii): remove when Physical Web C++ interface is provided.
struct UrlInfo {
  UrlInfo();
  UrlInfo(const UrlInfo& other);
  ~UrlInfo();
  base::Time scan_time;
  GURL site_url;
  std::string title;
  std::string description;
};

// Provides content suggestions from the Physical Web Service.
class PhysicalWebPageSuggestionsProvider : public ContentSuggestionsProvider {
 public:
  PhysicalWebPageSuggestionsProvider(
      ContentSuggestionsProvider::Observer* observer,
      CategoryFactory* category_factory);
  ~PhysicalWebPageSuggestionsProvider() override;

  void OnDisplayableUrlsChanged(const std::vector<UrlInfo>& urls);

  // ContentSuggestionsProvider implementation.
  CategoryStatus GetCategoryStatus(Category category) override;
  CategoryInfo GetCategoryInfo(Category category) override;
  void DismissSuggestion(const ContentSuggestion::ID& suggestion_id) override;
  void FetchSuggestionImage(const ContentSuggestion::ID& suggestion_id,
                            const ImageFetchedCallback& callback) override;
  void Fetch(const Category& category,
             const std::set<std::string>& known_suggestion_ids,
             const FetchDoneCallback& callback) override;
  void ClearHistory(
      base::Time begin,
      base::Time end,
      const base::Callback<bool(const GURL& url)>& filter) override;
  void ClearCachedSuggestions(Category category) override;
  void GetDismissedSuggestionsForDebugging(
      Category category,
      const DismissedSuggestionsCallback& callback) override;
  void ClearDismissedSuggestionsForDebugging(Category category) override;

 private:
  void NotifyStatusChanged(CategoryStatus new_status);

  CategoryStatus category_status_;
  const Category provided_category_;

  DISALLOW_COPY_AND_ASSIGN(PhysicalWebPageSuggestionsProvider);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_PHYSICAL_WEB_PAGES_PHYSICAL_WEB_PAGE_SUGGESTIONS_PROVIDER_H_
