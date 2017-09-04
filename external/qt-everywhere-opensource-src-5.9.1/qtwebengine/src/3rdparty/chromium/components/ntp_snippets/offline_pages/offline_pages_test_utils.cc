// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/offline_pages/offline_pages_test_utils.h"

#include <iterator>
#include <vector>

#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"

using offline_pages::ClientId;
using offline_pages::MultipleOfflinePageItemCallback;
using offline_pages::OfflinePageItem;
using offline_pages::StubOfflinePageModel;

namespace ntp_snippets {
namespace test {

FakeOfflinePageModel::FakeOfflinePageModel() {
  // This is to match StubOfflinePageModel behavior.
  is_loaded_ = true;
}

FakeOfflinePageModel::~FakeOfflinePageModel() {}

void FakeOfflinePageModel::GetPagesMatchingQuery(
    std::unique_ptr<offline_pages::OfflinePageModelQuery> query,
    const MultipleOfflinePageItemCallback& callback) {
  MultipleOfflinePageItemResult filtered_result;
  for (auto& item : items_) {
    if (query->Matches(item))
      filtered_result.emplace_back(item);
  }
  callback.Run(filtered_result);
}

void FakeOfflinePageModel::GetAllPages(
    const MultipleOfflinePageItemCallback& callback) {
  callback.Run(items_);
}

const std::vector<OfflinePageItem>& FakeOfflinePageModel::items() {
  return items_;
}

std::vector<OfflinePageItem>* FakeOfflinePageModel::mutable_items() {
  return &items_;
}

bool FakeOfflinePageModel::is_loaded() const {
  return is_loaded_;
}

void FakeOfflinePageModel::set_is_loaded(bool value) {
  is_loaded_ = value;
}

OfflinePageItem CreateDummyOfflinePageItem(int id,
                                           const std::string& name_space) {
  std::string id_string = base::IntToString(id);
  return OfflinePageItem(
      GURL("http://dummy.com/" + id_string), id,
      ClientId(name_space, base::GenerateGUID()),
      base::FilePath::FromUTF8Unsafe("some/folder/test" + id_string + ".mhtml"),
      0, base::Time::Now());
}

void CaptureDismissedSuggestions(
    std::vector<ContentSuggestion>* captured_suggestions,
    std::vector<ContentSuggestion> dismissed_suggestions) {
  std::move(dismissed_suggestions.begin(), dismissed_suggestions.end(),
            std::back_inserter(*captured_suggestions));
}

}  // namespace test
}  // namespace ntp_snippets
