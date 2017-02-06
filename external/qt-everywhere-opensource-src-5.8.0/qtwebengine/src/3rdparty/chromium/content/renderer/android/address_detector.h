// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_ADDRESS_DETECTOR_H_
#define CONTENT_RENDERER_ANDROID_ADDRESS_DETECTOR_H_

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/renderer/android/content_detector.h"
#include "url/gurl.h"

namespace content {

// Finds a geographical address (currently US only) in the given text string.
class AddressDetector : public ContentDetector {
 public:
  AddressDetector();
  ~AddressDetector() override;

 private:
  // Implementation of ContentDetector.
  bool FindContent(const base::string16::const_iterator& begin,
                   const base::string16::const_iterator& end,
                   size_t* start_pos,
                   size_t* end_pos,
                   std::string* content_text) override;
  GURL GetIntentURL(const std::string& content_text) override;
  size_t GetMaximumContentLength() override;

  std::string GetContentText(const base::string16& text);

  DISALLOW_COPY_AND_ASSIGN(AddressDetector);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_ADDRESS_DETECTOR_H_
