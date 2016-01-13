// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DATE_TIME_SUGGESTION_BUILDER_H_
#define CONTENT_RENDERER_DATE_TIME_SUGGESTION_BUILDER_H_

namespace blink {
struct WebDateTimeSuggestion;
}  // namespace blink

namespace content {
struct DateTimeSuggestion;

class DateTimeSuggestionBuilder {
 public:
  static DateTimeSuggestion Build(
      const blink::WebDateTimeSuggestion& suggestion);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DATE_TIME_SUGGESTION_BUILDER_H_
