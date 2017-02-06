// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTION_CATEGORY_H_
#define COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTION_CATEGORY_H_

namespace ntp_snippets {

// The category of a content suggestion. Note that even though these categories
// currently match the provider types, a provider type is not limited to provide
// suggestions of a single (fixed) category only. The category is used to
// determine where to display the suggestion.
enum class ContentSuggestionCategory : int { ARTICLE };

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTION_CATEGORY_H_
