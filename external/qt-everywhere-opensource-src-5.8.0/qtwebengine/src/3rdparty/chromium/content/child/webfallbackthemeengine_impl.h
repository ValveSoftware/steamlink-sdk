// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBFALLBACKTHEMEENGINE_IMPL_H_
#define CONTENT_CHILD_WEBFALLBACKTHEMEENGINE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebFallbackThemeEngine.h"

namespace content {

// This theme should only be used in layout tests in cases the mock theme can't
// handle (such as zoomed controls).
class WebFallbackThemeEngineImpl : public blink::WebFallbackThemeEngine {
 public:
  WebFallbackThemeEngineImpl();
  ~WebFallbackThemeEngineImpl();

  // WebFallbackThemeEngine methods:
  blink::WebSize getSize(blink::WebFallbackThemeEngine::Part) override;
  void paint(
      blink::WebCanvas* canvas,
      blink::WebFallbackThemeEngine::Part part,
      blink::WebFallbackThemeEngine::State state,
      const blink::WebRect& rect,
      const blink::WebFallbackThemeEngine::ExtraParams* extra_params) override;

 private:
  class WebFallbackNativeTheme;
  std::unique_ptr<WebFallbackNativeTheme> theme_;

  DISALLOW_COPY_AND_ASSIGN(WebFallbackThemeEngineImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBFALLBACKTHEMEENGINE_IMPL_H_
