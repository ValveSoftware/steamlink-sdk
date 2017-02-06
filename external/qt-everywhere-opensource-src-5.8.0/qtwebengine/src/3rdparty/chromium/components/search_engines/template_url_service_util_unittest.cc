// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<TemplateURLData> CreatePrepopulateTemplateURLData(
    int prepopulate_id,
    const std::string& keyword,
    TemplateURLID id) {
  std::unique_ptr<TemplateURLData> data(new TemplateURLData);
  data->prepopulate_id = prepopulate_id;
  data->SetKeyword(base::ASCIIToUTF16(keyword));
  data->id = id;
  return data;
}

// Creates a TemplateURL with default values except for the prepopulate ID,
// keyword and TemplateURLID. Only use this in tests if your tests do not
// care about other fields.
std::unique_ptr<TemplateURL> CreatePrepopulateTemplateURL(
    int prepopulate_id,
    const std::string& keyword,
    TemplateURLID id) {
  return base::WrapUnique(new TemplateURL(
      *CreatePrepopulateTemplateURLData(prepopulate_id, keyword, id)));
}

};  // namespace

TEST(TemplateURLServiceUtilTest, RemoveDuplicatePrepopulateIDs) {
  ScopedVector<TemplateURLData> prepopulated_turls;
  TemplateURLService::TemplateURLVector local_turls;
  STLElementDeleter<TemplateURLService::TemplateURLVector> local_turls_deleter(
      &local_turls);

  prepopulated_turls.push_back(
      CreatePrepopulateTemplateURLData(1, "winner4", 1).release());
  prepopulated_turls.push_back(
      CreatePrepopulateTemplateURLData(2, "xxx", 2).release());
  prepopulated_turls.push_back(
      CreatePrepopulateTemplateURLData(3, "yyy", 3).release());

  // Create a sets of different TURLs grouped by prepopulate ID. Each group
  // will test a different heuristic of RemoveDuplicatePrepopulateIDs.
  // Ignored set - These should be left alone as they do not have valid
  // prepopulate IDs.
  local_turls.push_back(
      CreatePrepopulateTemplateURL(0, "winner1", 4).release());
  local_turls.push_back(
      CreatePrepopulateTemplateURL(0, "winner2", 5).release());
  local_turls.push_back(
      CreatePrepopulateTemplateURL(0, "winner3", 6).release());
  size_t num_non_prepopulated_urls = local_turls.size();

  // Keyword match set - Prefer the one that matches the keyword of the
  // prepopulate ID.
  local_turls.push_back(CreatePrepopulateTemplateURL(1, "loser1", 7).release());
  local_turls.push_back(CreatePrepopulateTemplateURL(1, "loser2", 8).release());
  local_turls.push_back(
      CreatePrepopulateTemplateURL(1, "winner4", 9).release());

  // Default set - Prefer the default search engine over all other criteria.
  // The last one is the default. It will be passed as the
  // default_search_provider parameter to RemoveDuplicatePrepopulateIDs.
  local_turls.push_back(
      CreatePrepopulateTemplateURL(2, "loser3", 10).release());
  local_turls.push_back(CreatePrepopulateTemplateURL(2, "xxx", 11).release());
  TemplateURL* default_turl =
      CreatePrepopulateTemplateURL(2, "winner5", 12).release();
  local_turls.push_back(default_turl);

  // ID set - Prefer the lowest TemplateURLID if the keywords don't match and if
  // none are the default.
  local_turls.push_back(
      CreatePrepopulateTemplateURL(3, "winner6", 13).release());
  local_turls.push_back(
      CreatePrepopulateTemplateURL(3, "loser5", 14).release());
  local_turls.push_back(
      CreatePrepopulateTemplateURL(3, "loser6", 15).release());

  RemoveDuplicatePrepopulateIDs(NULL, prepopulated_turls, default_turl,
                                &local_turls, SearchTermsData(), NULL);

  // Verify that the expected local TURLs survived the process.
  EXPECT_EQ(local_turls.size(),
            prepopulated_turls.size() + num_non_prepopulated_urls);
  for (TemplateURLService::TemplateURLVector::const_iterator itr =
       local_turls.begin(); itr != local_turls.end(); ++itr) {
    EXPECT_TRUE(base::StartsWith((*itr)->keyword(),
                                 base::ASCIIToUTF16("winner"),
                                 base::CompareCase::SENSITIVE));
  }
}
