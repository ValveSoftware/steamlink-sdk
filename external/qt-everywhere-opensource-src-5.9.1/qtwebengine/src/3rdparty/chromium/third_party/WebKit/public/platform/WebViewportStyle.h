// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebViewportStyle_h
#define WebViewportStyle_h

namespace blink {

// UA style if viewport is enabled.
enum class WebViewportStyle {
  Default,
  // Includes viewportAndroid.css.
  Mobile,
  // Includes viewportTelevision.css.
  Television
};

}  // namespace blink

#endif  // WebViewportStyle_h
