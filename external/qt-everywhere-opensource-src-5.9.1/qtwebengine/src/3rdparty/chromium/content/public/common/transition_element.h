// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_TRANSITION_ELEMENT_H_
#define CONTENT_PUBLIC_COMMON_TRANSITION_ELEMENT_H_

#include <string>

#include "content/common/content_export.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

// This struct stores the information of one transition element.
struct CONTENT_EXPORT TransitionElement {
  std::string id;
  gfx::Rect rect;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_TRANSITION_ELEMENT_H_
