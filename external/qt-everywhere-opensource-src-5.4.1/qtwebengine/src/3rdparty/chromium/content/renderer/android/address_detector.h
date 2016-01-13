// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_ADDRESS_DETECTOR_H_
#define CONTENT_RENDERER_ANDROID_ADDRESS_DETECTOR_H_

#include "base/compiler_specific.h"
#include "content/renderer/android/content_detector.h"
#include "url/gurl.h"

namespace content {

// Finds a geographical address (currently US only) in the given text string.
class AddressDetector : public ContentDetector {
 public:
  AddressDetector();
  virtual ~AddressDetector();

 private:
  // Implementation of ContentDetector.
  virtual bool FindContent(const base::string16::const_iterator& begin,
                           const base::string16::const_iterator& end,
                           size_t* start_pos,
                           size_t* end_pos,
                           std::string* content_text) OVERRIDE;
  virtual GURL GetIntentURL(const std::string& content_text) OVERRIDE;
  virtual size_t GetMaximumContentLength() OVERRIDE;

  std::string GetContentText(const base::string16& text);

  DISALLOW_COPY_AND_ASSIGN(AddressDetector);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_ADDRESS_DETECTOR_H_
