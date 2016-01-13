// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/date_time_suggestion_builder.h"

#include "content/common/date_time_suggestion.h"
#include "third_party/WebKit/public/web/WebDateTimeSuggestion.h"

namespace content {

// static
DateTimeSuggestion DateTimeSuggestionBuilder::Build(
    const blink::WebDateTimeSuggestion& suggestion) {
  DateTimeSuggestion result;
  result.value = suggestion.value;
  result.localized_value = suggestion.localizedValue;
  result.label = suggestion.label;
  return result;
}

}  // namespace content
