// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_FEEDBACK_BLIMP_FEEDBACK_DATA_H_
#define BLIMP_CLIENT_CORE_FEEDBACK_BLIMP_FEEDBACK_DATA_H_

#include <string>
#include <unordered_map>

namespace blimp {
namespace client {
class BlimpContentsManager;

// Denotes whether Blimp is supported in the current.
extern const char kFeedbackSupportedKey[];
// Denotes whether there exists any visible BlimpContents.
extern const char kFeedbackHasVisibleBlimpContents[];
// Denotes the user name for the current logged in user.
extern const char kFeedbackUserNameKey[];

// Creates a data object containing data about Blimp to be used for feedback.
std::unordered_map<std::string, std::string> CreateBlimpFeedbackData(
    BlimpContentsManager* blimp_contents_manager,
    const std::string& username);

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_FEEDBACK_BLIMP_FEEDBACK_DATA_H_
