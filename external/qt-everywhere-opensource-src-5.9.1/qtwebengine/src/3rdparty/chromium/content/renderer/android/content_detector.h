// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_CONTENT_DETECTOR_H_
#define CONTENT_RENDERER_ANDROID_CONTENT_DETECTOR_H_

#include <stddef.h>

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "url/gurl.h"

namespace blink {
class WebHitTestResult;
}

namespace content {

// Base class for text-based content detectors.
class ContentDetector {
 public:
  virtual ~ContentDetector() {}

  // Returns an intent URL for the content around the hit_test.
  // If no content is found, an empty URL will be returned.
  blink::WebURL FindTappedContent(const blink::WebHitTestResult& hit_test);

 protected:
  ContentDetector() {}

  // Parses the input string defined by the begin/end iterators returning true
  // if the desired content is found. The start and end positions relative to
  // the input iterators are returned in start_pos and end_pos.
  // The end position is assumed to be non-inclusive.
  virtual bool FindContent(const base::string16::const_iterator& begin,
                           const base::string16::const_iterator& end,
                           size_t* start_pos,
                           size_t* end_pos,
                           std::string* content_text) = 0;

  // Returns the intent URL that should process the content, if any.
  virtual GURL GetIntentURL(const std::string& content_text) = 0;

  // Returns the maximum length of text to be extracted around the tapped
  // position in order to search for content.
  virtual size_t GetMaximumContentLength() = 0;

  bool FindContentRange(const blink::WebHitTestResult& hit_test,
                        std::string* content_text);

  DISALLOW_COPY_AND_ASSIGN(ContentDetector);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_CONTENT_DETECTOR_H_
