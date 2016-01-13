// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/color_suggestion.h"

#include "third_party/WebKit/public/web/WebColorSuggestion.h"

namespace content {

ColorSuggestion::ColorSuggestion(const blink::WebColorSuggestion& suggestion)
    : color(suggestion.color),
      label(suggestion.label) {
}

}  // namespace content
