// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_PUBLIC_CONTENTS_BLIMP_CONTENTS_VIEW_H_
#define BLIMP_CLIENT_PUBLIC_CONTENTS_BLIMP_CONTENTS_VIEW_H_

#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class Layer;
}  // namespace cc

namespace gfx {
class Size;
}  // namespace gfx

namespace ui {
class MotionEvent;
}

namespace blimp {
namespace client {

// A BlimpContentsView exposes the classes required to visually represent a
// BlimpContents (gfx::NativeView and cc::Layer).  It also exposes methods that
// let the embedder interact with the page content.
class BlimpContentsView {
 public:
  using ReadbackRequestCallback =
      base::Callback<void(std::unique_ptr<SkBitmap>)>;

  virtual ~BlimpContentsView() = default;

  // Returns the platform specific NativeView that represents the contents of
  // this BlimpContents.
  virtual gfx::NativeView GetNativeView() = 0;

  // Returns the CC Layer that represents the contents of this BlimpContents.
  // This can be used instead of |GetNativeView()| if only the CC Layer is
  // required.
  virtual scoped_refptr<cc::Layer> GetLayer() = 0;

  // Called to set both the size and the scale of the BlimpContents.  |size| is
  // in pixels and |device_scale_factor| is in terms of dp to px.
  virtual void SetSizeAndScale(const gfx::Size& size,
                               float device_scale_factor) = 0;

  // Called to pass a ui::MotionEvent to the BlimpContents.  Returns whether or
  // not |motion_event| was handled.
  virtual bool OnTouchEvent(const ui::MotionEvent& motion_event) = 0;

  // Copies the current visible content from the BlimpContents and returns a
  // SkBitmap through |callback|.  If the readback fails |nullptr| is returned
  // instead.
  virtual void CopyFromCompositingSurface(
      const ReadbackRequestCallback& callback) = 0;

 protected:
  BlimpContentsView() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpContentsView);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_PUBLIC_CONTENTS_BLIMP_CONTENTS_VIEW_H_
