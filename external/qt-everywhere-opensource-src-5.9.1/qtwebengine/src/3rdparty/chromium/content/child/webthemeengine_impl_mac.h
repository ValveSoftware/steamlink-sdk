// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBTHEMEENGINE_IMPL_MAC_H_
#define CONTENT_CHILD_WEBTHEMEENGINE_IMPL_MAC_H_

#include "third_party/WebKit/public/platform/WebThemeEngine.h"

namespace content {

class WebThemeEngineImpl : public blink::WebThemeEngine {
#if defined(USE_APPSTORE_COMPLIANT_CODE)
public:
 // WebThemeEngine methods:
 blink::WebSize getSize(blink::WebThemeEngine::Part) override;
 void paint(blink::WebCanvas* canvas,
            blink::WebThemeEngine::Part part,
            blink::WebThemeEngine::State state,
            const blink::WebRect& rect,
            const blink::WebThemeEngine::ExtraParams* extra_params) override;
 virtual void paintStateTransition(blink::WebCanvas* canvas,
                                   blink::WebThemeEngine::Part part,
                                   blink::WebThemeEngine::State startState,
                                   blink::WebThemeEngine::State endState,
                                   double progress,
                                   const blink::WebRect& rect);
#endif
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBTHEMEENGINE_IMPL_MAC_H_
