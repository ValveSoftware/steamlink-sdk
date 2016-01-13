// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_WEB_PREFERENCES_H_
#define CONTENT_PUBLIC_RENDERER_WEB_PREFERENCES_H_

#include "content/common/content_export.h"

struct WebPreferences;

namespace blink {
class WebView;
}

namespace content {

CONTENT_EXPORT void ApplyWebPreferences(const WebPreferences& prefs,
                                        blink::WebView* web_view);

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_WEB_PREFERENCES_H_
