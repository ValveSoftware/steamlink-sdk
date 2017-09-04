// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_constants.h"

namespace ntp_snippets {

// Also defined in SnippetArticleViewHolder.java
const char kStudyName[] = "NTPSnippets";

const base::FilePath::CharType kDatabaseFolder[] =
    FILE_PATH_LITERAL("NTPSnippets");

const char kChromeReaderServer[] =
    "https://chromereader-pa.googleapis.com/v1/fetch";
const char kContentSuggestionsServer[] =
    "https://chromecontentsuggestions-pa.googleapis.com/v1/suggestions/fetch";
const char kContentSuggestionsStagingServer[] =
    "https://staging-chromecontentsuggestions-pa.googleapis.com/v1/suggestions/fetch";
const char kContentSuggestionsAlphaServer[] =
    "https://alpha-chromecontentsuggestions-pa.sandbox.googleapis.com/v1/suggestions/fetch";

}  // namespace ntp_snippets
