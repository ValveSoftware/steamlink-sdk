// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/window_container_type.h"

#include "base/strings/string_util.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebWindowFeatures.h"

namespace {

const char kBackground[] = "background";
const char kPersistent[] = "persistent";

}  // namespace

WindowContainerType WindowFeaturesToContainerType(
    const blink::WebWindowFeatures& window_features) {
  bool background = false;
  bool persistent = false;

  for (size_t i = 0; i < window_features.additionalFeatures.size(); ++i) {
    if (LowerCaseEqualsASCII(window_features.additionalFeatures[i],
                             kBackground))
      background = true;
    else if (LowerCaseEqualsASCII(window_features.additionalFeatures[i],
                                  kPersistent))
      persistent = true;
  }

  if (background) {
    if (persistent)
      return WINDOW_CONTAINER_TYPE_PERSISTENT;
    else
      return WINDOW_CONTAINER_TYPE_BACKGROUND;
  } else {
    return WINDOW_CONTAINER_TYPE_NORMAL;
  }
}
