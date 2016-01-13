// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_PHONE_NUMBER_DETECTOR_H_
#define CONTENT_RENDERER_ANDROID_PHONE_NUMBER_DETECTOR_H_

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/renderer/android/content_detector.h"

namespace content {

class PhoneNumberDetectorTest;

// Finds a telephone number in the given content text string.
class CONTENT_EXPORT PhoneNumberDetector : public ContentDetector {
 public:
  PhoneNumberDetector();
  explicit PhoneNumberDetector(const std::string& region);
  virtual ~PhoneNumberDetector();

 private:
  friend class PhoneNumberDetectorTest;

  // Implementation of ContentDetector.
  virtual bool FindContent(const base::string16::const_iterator& begin,
                           const base::string16::const_iterator& end,
                           size_t* start_pos,
                           size_t* end_pos,
                           std::string* content_text) OVERRIDE;
  virtual GURL GetIntentURL(const std::string& content_text) OVERRIDE;
  virtual size_t GetMaximumContentLength() OVERRIDE;

  const std::string region_code_;

  DISALLOW_COPY_AND_ASSIGN(PhoneNumberDetector);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_PHONE_NUMBER_DETECTOR_H_
