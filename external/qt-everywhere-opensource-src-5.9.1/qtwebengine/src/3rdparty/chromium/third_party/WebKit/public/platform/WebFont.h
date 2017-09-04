// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFont_h
#define WebFont_h

#include "WebCanvas.h"
#include "WebColor.h"
#include "WebCommon.h"
#include <memory>

namespace blink {

struct WebFloatPoint;
struct WebFloatRect;
struct WebFontDescription;
struct WebRect;
struct WebTextRun;

class WebFont {
 public:
  BLINK_PLATFORM_EXPORT static WebFont* create(const WebFontDescription&);
  BLINK_PLATFORM_EXPORT ~WebFont();

  BLINK_PLATFORM_EXPORT WebFontDescription getFontDescription() const;
  BLINK_PLATFORM_EXPORT int ascent() const;
  BLINK_PLATFORM_EXPORT int descent() const;
  BLINK_PLATFORM_EXPORT int height() const;
  BLINK_PLATFORM_EXPORT int lineSpacing() const;
  BLINK_PLATFORM_EXPORT float xHeight() const;
  BLINK_PLATFORM_EXPORT void drawText(WebCanvas*,
                                      const WebTextRun&,
                                      const WebFloatPoint& leftBaseline,
                                      WebColor,
                                      const WebRect& clip) const;
  BLINK_PLATFORM_EXPORT int calculateWidth(const WebTextRun&) const;
  BLINK_PLATFORM_EXPORT int offsetForPosition(const WebTextRun&,
                                              float position) const;
  BLINK_PLATFORM_EXPORT WebFloatRect
  selectionRectForText(const WebTextRun&,
                       const WebFloatPoint& leftBaseline,
                       int height,
                       int from = 0,
                       int to = -1) const;

 private:
  explicit WebFont(const WebFontDescription&);

  class Impl;
  std::unique_ptr<Impl> m_private;
};

}  // namespace blink

#endif
