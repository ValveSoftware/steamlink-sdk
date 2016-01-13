// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBTHEMEENGINE_IMPL_MAC_H_
#define CONTENT_CHILD_WEBTHEMEENGINE_IMPL_MAC_H_

#include "third_party/WebKit/public/platform/WebThemeEngine.h"

namespace content {

class WebThemeEngineImpl : public blink::WebThemeEngine {
 public:
  // blink::WebThemeEngine implementation.
  virtual void paintScrollbarThumb(
      blink::WebCanvas* canvas,
      blink::WebThemeEngine::State part,
      blink::WebThemeEngine::Size state,
      const blink::WebRect& rect,
      const blink::WebThemeEngine::ScrollbarInfo& extra_params);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBTHEMEENGINE_IMPL_MAC_H_
