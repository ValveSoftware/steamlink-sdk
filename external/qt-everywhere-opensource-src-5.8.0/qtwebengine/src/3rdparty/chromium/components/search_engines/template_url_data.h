// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_DATA_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_DATA_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/search_engines/template_url_id.h"
#include "url/gurl.h"

// The data for the TemplateURL.  Separating this into its own class allows most
// users to do SSA-style usage of TemplateURL: construct a TemplateURLData with
// whatever fields are desired, then create an immutable TemplateURL from it.
struct TemplateURLData {
  TemplateURLData();
  TemplateURLData(const TemplateURLData& other);
  ~TemplateURLData();

  // A short description of the template. This is the name we show to the user
  // in various places that use TemplateURLs. For example, the location bar
  // shows this when the user selects a substituting match.
  void SetShortName(const base::string16& short_name);
  const base::string16& short_name() const { return short_name_; }

  // The shortcut for this TemplateURL.  |keyword| must be non-empty.
  void SetKeyword(const base::string16& keyword);
  const base::string16& keyword() const { return keyword_; }

  // The raw URL for the TemplateURL, which may not be valid as-is (e.g. because
  // it requires substitutions first).  This must be non-empty.
  void SetURL(const std::string& url);
  const std::string& url() const { return url_; }

  // Optional additional raw URLs.
  std::string suggestions_url;
  std::string instant_url;
  std::string image_url;
  std::string new_tab_url;
  std::string contextual_search_url;

  // The following post_params are comma-separated lists used to specify the
  // post parameters for the corresponding URL.
  std::string search_url_post_params;
  std::string suggestions_url_post_params;
  std::string instant_url_post_params;
  std::string image_url_post_params;

  // Optional favicon for the TemplateURL.
  GURL favicon_url;

  // URL to the OSD file this came from. May be empty.
  GURL originating_url;

  // Whether this TemplateURL is shown in the default list of search providers.
  // This is just a property and does not indicate whether the TemplateURL has a
  // TemplateURLRef that supports replacement. Use
  // TemplateURL::ShowInDefaultList() to test both.
  bool show_in_default_list;

  // Whether it's safe for auto-modification code (the autogenerator and the
  // code that imports data from other browsers) to replace the TemplateURL.
  // This should be set to false for any TemplateURL the user edits, or any
  // TemplateURL that the user clearly manually edited in the past, like a
  // bookmark keyword from another browser.
  bool safe_for_autoreplace;

  // The list of supported encodings for the search terms. This may be empty,
  // which indicates the terms should be encoded with UTF-8.
  std::vector<std::string> input_encodings;

  // Unique identifier of this TemplateURL. The unique ID is set by the
  // TemplateURLService when the TemplateURL is added to it.
  TemplateURLID id;

  // Date this TemplateURL was created.
  //
  // NOTE: this may be 0, which indicates the TemplateURL was created before we
  // started tracking creation time.
  base::Time date_created;

  // The last time this TemplateURL was modified by a user, since creation.
  //
  // NOTE: Like date_created above, this may be 0.
  base::Time last_modified;

  // True if this TemplateURL was automatically created by the administrator via
  // group policy.
  bool created_by_policy;

  // Number of times this TemplateURL has been explicitly used to load a URL.
  // We don't increment this for uses as the "default search engine" since
  // that's not really "explicit" usage and incrementing would result in pinning
  // the user's default search engine(s) to the top of the list of searches on
  // the New Tab page, de-emphasizing the omnibox as "where you go to search".
  int usage_count;

  // If this TemplateURL comes from prepopulated data the prepopulate_id is > 0.
  int prepopulate_id;

  // The primary unique identifier for Sync. This set on all TemplateURLs
  // regardless of whether they have been associated with Sync.
  std::string sync_guid;

  // A list of URL patterns that can be used, in addition to |url_|, to extract
  // search terms from a URL.
  std::vector<std::string> alternate_urls;

  // A parameter that, if present in the query or ref parameters of a search_url
  // or instant_url, causes Chrome to replace the URL with the search term.
  std::string search_terms_replacement_key;

 private:
  // Private so we can enforce using the setters and thus enforce that these
  // fields are never empty.
  base::string16 short_name_;
  base::string16 keyword_;
  std::string url_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_DATA_H_
