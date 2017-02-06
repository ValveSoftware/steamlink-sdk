// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_PROVIDER_TYPE_H_
#define COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_PROVIDER_TYPE_H_

namespace ntp_snippets {

// A type of content suggestion provider. For each of these types, there will be
// at most one provider instance registered and running. Note that these
// provider types do not necessarily match the suggestion categories. The
// provider type is used to identify the source of a suggestion and to direct
// calls from the UI like discarding back to the right provider.
enum class ContentSuggestionsProviderType : int { ARTICLES };

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_PROVIDER_TYPE_H_
