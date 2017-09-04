// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/feedback/blimp_feedback_data.h"

#include "blimp/client/core/contents/blimp_contents_manager.h"

namespace blimp {
namespace client {
const char kFeedbackSupportedKey[] = "Blimp Supported";
const char kFeedbackHasVisibleBlimpContents[] = "Blimp Visible";
const char kFeedbackUserNameKey[] = "Blimp Account";

namespace {
std::string HasVisibleBlimpContents(
    BlimpContentsManager* blimp_contents_manager) {
  std::vector<BlimpContentsImpl*> all_blimp_contents =
      blimp_contents_manager->GetAllActiveBlimpContents();
  for (const auto& item : all_blimp_contents) {
    if (item->document_manager()->visible()) {
      return "true";
    }
  }
  return "false";
}
}  // namespace

std::unordered_map<std::string, std::string> CreateBlimpFeedbackData(
    BlimpContentsManager* blimp_contents_manager,
    const std::string& username) {
  std::unordered_map<std::string, std::string> data;
  data.insert(std::make_pair(kFeedbackSupportedKey, "true"));
  data.insert(std::make_pair(kFeedbackHasVisibleBlimpContents,
                             HasVisibleBlimpContents(blimp_contents_manager)));
  data.insert(std::make_pair(kFeedbackUserNameKey, username));
  return data;
}

}  // namespace client
}  // namespace blimp
