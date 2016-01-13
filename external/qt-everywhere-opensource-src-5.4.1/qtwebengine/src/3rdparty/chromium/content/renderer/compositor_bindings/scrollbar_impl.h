// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_BINDINGS_SCROLLBAR_IMPL_H_
#define CONTENT_RENDERER_COMPOSITOR_BINDINGS_SCROLLBAR_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/input/scrollbar.h"
#include "third_party/WebKit/public/platform/WebScrollbarThemePainter.h"

namespace blink {
class WebScrollbar;
class WebScrollbarThemeGeometry;
}

namespace content {

class ScrollbarImpl : public cc::Scrollbar {
 public:
  ScrollbarImpl(scoped_ptr<blink::WebScrollbar> scrollbar,
                blink::WebScrollbarThemePainter painter,
                scoped_ptr<blink::WebScrollbarThemeGeometry> geometry);
  virtual ~ScrollbarImpl();

  // cc::Scrollbar implementation.
  virtual cc::ScrollbarOrientation Orientation() const OVERRIDE;
  virtual bool IsLeftSideVerticalScrollbar() const OVERRIDE;
  virtual bool HasThumb() const OVERRIDE;
  virtual bool IsOverlay() const OVERRIDE;
  virtual gfx::Point Location() const OVERRIDE;
  virtual int ThumbThickness() const OVERRIDE;
  virtual int ThumbLength() const OVERRIDE;
  virtual gfx::Rect TrackRect() const OVERRIDE;
  virtual void PaintPart(SkCanvas* canvas,
                         cc::ScrollbarPart part,
                         const gfx::Rect& content_rect) OVERRIDE;

 private:
  scoped_ptr<blink::WebScrollbar> scrollbar_;
  blink::WebScrollbarThemePainter painter_;
  scoped_ptr<blink::WebScrollbarThemeGeometry> geometry_;

  DISALLOW_COPY_AND_ASSIGN(ScrollbarImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_BINDINGS_SCROLLBAR_IMPL_H_

