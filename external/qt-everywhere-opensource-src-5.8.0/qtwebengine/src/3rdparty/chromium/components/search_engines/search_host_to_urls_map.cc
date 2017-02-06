// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_host_to_urls_map.h"

#include <memory>

#include "components/search_engines/template_url.h"

SearchHostToURLsMap::SearchHostToURLsMap()
    : initialized_(false) {
}

SearchHostToURLsMap::~SearchHostToURLsMap() {
}

void SearchHostToURLsMap::Init(
    const TemplateURLService::TemplateURLVector& template_urls,
    const SearchTermsData& search_terms_data) {
  DCHECK(!initialized_);
  initialized_ = true;  // Set here so Add doesn't assert.
  Add(template_urls, search_terms_data);
}

void SearchHostToURLsMap::Add(TemplateURL* template_url,
                              const SearchTermsData& search_terms_data) {
  DCHECK(initialized_);
  DCHECK(template_url);
  DCHECK_NE(TemplateURL::OMNIBOX_API_EXTENSION, template_url->GetType());

  const GURL url(template_url->GenerateSearchURL(search_terms_data));
  if (!url.is_valid() || !url.has_host())
    return;

  host_to_urls_map_[url.host()].insert(template_url);
}

void SearchHostToURLsMap::Remove(TemplateURL* template_url) {
  DCHECK(initialized_);
  DCHECK(template_url);
  DCHECK_NE(TemplateURL::OMNIBOX_API_EXTENSION, template_url->GetType());

  for (HostToURLsMap::iterator i = host_to_urls_map_.begin();
       i != host_to_urls_map_.end(); ++i) {
    TemplateURLSet::iterator url_set_iterator = i->second.find(template_url);
    if (url_set_iterator != i->second.end()) {
      i->second.erase(url_set_iterator);
      if (i->second.empty())
        host_to_urls_map_.erase(i);
      // A given TemplateURL only occurs once in the map. As soon as we find the
      // entry, stop.
      return;
    }
  }
}

TemplateURL* SearchHostToURLsMap::GetTemplateURLForHost(
    const std::string& host) {
  DCHECK(initialized_);

  HostToURLsMap::const_iterator iter = host_to_urls_map_.find(host);
  if (iter == host_to_urls_map_.end() || iter->second.empty())
    return NULL;
  return *(iter->second.begin());  // Return the 1st element.
}

SearchHostToURLsMap::TemplateURLSet* SearchHostToURLsMap::GetURLsForHost(
    const std::string& host) {
  DCHECK(initialized_);

  HostToURLsMap::iterator urls_for_host = host_to_urls_map_.find(host);
  if (urls_for_host == host_to_urls_map_.end() || urls_for_host->second.empty())
    return NULL;
  return &urls_for_host->second;
}

void SearchHostToURLsMap::Add(
    const TemplateURLService::TemplateURLVector& template_urls,
    const SearchTermsData& search_terms_data) {
  for (TemplateURLService::TemplateURLVector::const_iterator i(
       template_urls.begin()); i != template_urls.end(); ++i)
    Add(*i, search_terms_data);
}
