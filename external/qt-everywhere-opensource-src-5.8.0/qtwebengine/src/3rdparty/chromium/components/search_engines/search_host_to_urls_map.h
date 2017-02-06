// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_HOST_TO_URLS_MAP_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_HOST_TO_URLS_MAP_H_

#include <map>
#include <set>
#include <string>

#include "base/macros.h"
#include "components/search_engines/template_url_service.h"

// Holds the host to template url mappings for the search providers. WARNING:
// This class does not own any TemplateURLs passed to it and it is up to the
// caller to ensure the right lifetime of them.
class SearchHostToURLsMap {
 public:
  typedef std::set<TemplateURL*> TemplateURLSet;

  SearchHostToURLsMap();
  ~SearchHostToURLsMap();

  // Initializes the map.
  void Init(const TemplateURLService::TemplateURLVector& template_urls,
            const SearchTermsData& search_terms_data);

  // Adds a new TemplateURL to the map. Since |template_url| is owned
  // externally, Remove or RemoveAll should be called if it becomes invalid.
  void Add(TemplateURL* template_url,
           const SearchTermsData& search_terms_data);

  // Removes the TemplateURL from the lookup.
  void Remove(TemplateURL* template_url);

  // Returns the first TemplateURL found with a URL using the specified |host|,
  // or NULL if there are no such TemplateURLs
  TemplateURL* GetTemplateURLForHost(const std::string& host);

  // Return the TemplateURLSet for the given the |host| or NULL if there are
  // none.
  TemplateURLSet* GetURLsForHost(const std::string& host);

 private:
  friend class SearchHostToURLsMapTest;

  typedef std::map<std::string, TemplateURLSet> HostToURLsMap;

  // Adds many URLs to the map.
  void Add(const TemplateURLService::TemplateURLVector& template_urls,
           const SearchTermsData& search_terms_data);

  // Maps from host to set of TemplateURLs whose search url host is host.
  HostToURLsMap host_to_urls_map_;

  // The security origin for the default search provider.
  std::string default_search_origin_;

  // Has Init been called?
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(SearchHostToURLsMap);
};

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_HOST_TO_URLS_MAP_H_
