// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBFALLBACKTHEMEENGINE_IMPL_H_
#define CONTENT_CHILD_WEBFALLBACKTHEMEENGINE_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebFallbackThemeEngine.h"

namespace ui {
class FallbackTheme;
}

namespace content {

class WebFallbackThemeEngineImpl : public blink::WebFallbackThemeEngine {
 public:
  WebFallbackThemeEngineImpl();
  virtual ~WebFallbackThemeEngineImpl();

  // WebFallbackThemeEngine methods:
  virtual blink::WebSize getSize(blink::WebFallbackThemeEngine::Part);
  virtual void paint(
      blink::WebCanvas* canvas,
      blink::WebFallbackThemeEngine::Part part,
      blink::WebFallbackThemeEngine::State state,
      const blink::WebRect& rect,
      const blink::WebFallbackThemeEngine::ExtraParams* extra_params);

 private:
  scoped_ptr<ui::FallbackTheme> theme_;

  DISALLOW_COPY_AND_ASSIGN(WebFallbackThemeEngineImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBFALLBACKTHEMEENGINE_IMPL_H_
