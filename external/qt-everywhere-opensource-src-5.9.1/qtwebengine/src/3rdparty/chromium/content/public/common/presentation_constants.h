// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PRESENTATION_CONSTANTS_H_
#define CONTENT_PUBLIC_COMMON_PRESENTATION_CONSTANTS_H_

#include <stddef.h>         // For size_t

#include "content/common/content_export.h"

namespace content {

// The maximum number of bytes allowed in a presentation session message.
CONTENT_EXPORT extern const size_t kMaxPresentationSessionMessageSize;

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PRESENTATION_CONSTANTS_H_
