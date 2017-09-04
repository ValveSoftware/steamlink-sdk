// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_ANDROID_CONTENT_DETECTION_PREFIXES_H_
#define CONTENT_PUBLIC_RENDERER_ANDROID_CONTENT_DETECTION_PREFIXES_H_

#include "content/common/content_export.h"

namespace content {

// Android intent url prefixes for content detection.
// Note that these do not work with GURL::SchemeIs.
CONTENT_EXPORT extern const char kAddressPrefix[];
CONTENT_EXPORT extern const char kEmailPrefix[];
CONTENT_EXPORT extern const char kPhoneNumberPrefix[];

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_ANDROID_CONTENT_DETECTION_PREFIXES_H_

