// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/content_detector.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/web/WebHitTestResult.h"
#include "third_party/WebKit/public/web/WebSurroundingText.h"

using blink::WebRange;
using blink::WebSurroundingText;

namespace content {

ContentDetector::Result::Result() : valid(false) {}

ContentDetector::Result::Result(const blink::WebRange& content_boundaries,
                                const std::string& text,
                                const GURL& intent_url)
  : valid(true),
    content_boundaries(content_boundaries),
    text(text),
    intent_url(intent_url) {
}

ContentDetector::Result::~Result() {}

ContentDetector::Result ContentDetector::FindTappedContent(
    const blink::WebHitTestResult& hit_test) {
  if (hit_test.isNull())
    return Result();

  std::string content_text;
  blink::WebRange range = FindContentRange(hit_test, &content_text);
  if (range.isNull())
    return Result();

  GURL intent_url = GetIntentURL(content_text);
  return Result(range, content_text, intent_url);
}

WebRange ContentDetector::FindContentRange(
    const blink::WebHitTestResult& hit_test,
    std::string* content_text) {
  // As the surrounding text extractor looks at maxLength/2 characters on
  // either side of the hit point, we need to double max content length here.
  WebSurroundingText surrounding_text;
  surrounding_text.initialize(hit_test.node(), hit_test.localPoint(),
                              GetMaximumContentLength() * 2);
  if (surrounding_text.isNull())
    return WebRange();

  base::string16 content = surrounding_text.textContent();
  if (content.empty())
    return WebRange();

  size_t selected_offset = surrounding_text.hitOffsetInTextContent();
  for (size_t start_offset = 0; start_offset < content.length();) {
    size_t relative_start, relative_end;
    if (!FindContent(content.begin() + start_offset,
        content.end(), &relative_start, &relative_end, content_text)) {
      break;
    } else {
      size_t content_start = start_offset + relative_start;
      size_t content_end = start_offset + relative_end;
      DCHECK(content_end <= content.length());

      if (selected_offset >= content_start && selected_offset < content_end) {
        WebRange range = surrounding_text.rangeFromContentOffsets(
            content_start, content_end);
        DCHECK(!range.isNull());
        return range;
      } else {
        start_offset += relative_end;
      }
    }
  }

  return WebRange();
}

}  // namespace content
