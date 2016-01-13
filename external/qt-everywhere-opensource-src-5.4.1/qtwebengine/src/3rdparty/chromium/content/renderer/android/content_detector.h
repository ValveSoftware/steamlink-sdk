// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_CONTENT_DETECTOR_H_
#define CONTENT_RENDERER_ANDROID_CONTENT_DETECTOR_H_

#include "third_party/WebKit/public/web/WebRange.h"
#include "url/gurl.h"

namespace blink {
class WebHitTestResult;
}

namespace content {

// Base class for text-based content detectors.
class ContentDetector {
 public:
  // Holds the content detection results.
  struct Result {
    Result();
    Result(const blink::WebRange& content_boundaries,
           const std::string& text,
           const GURL& intent_url);
    ~Result();

    bool valid;
    blink::WebRange content_boundaries;
    std::string text; // Processed text of the content.
    GURL intent_url; // URL of the intent that should process this content.
  };

  virtual ~ContentDetector() {}

  // Returns a WebKit range delimiting the contents found around the tapped
  // position. If no content is found a null range will be returned.
  Result FindTappedContent(const blink::WebHitTestResult& hit_test);

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

  blink::WebRange FindContentRange(const blink::WebHitTestResult& hit_test,
                                    std::string* content_text);

  DISALLOW_COPY_AND_ASSIGN(ContentDetector);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_CONTENT_DETECTOR_H_
