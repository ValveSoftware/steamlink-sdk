// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/software_output_device_x11.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/x/x11_types.h"

namespace content {

SoftwareOutputDeviceX11::SoftwareOutputDeviceX11(ui::Compositor* compositor)
    : compositor_(compositor), display_(gfx::GetXDisplay()), gc_(NULL) {
  // TODO(skaslev) Remove this when crbug.com/180702 is fixed.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  gc_ = XCreateGC(display_, compositor_->widget(), 0, NULL);
  if (!XGetWindowAttributes(display_, compositor_->widget(), &attributes_)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window "
               << compositor_->widget();
    return;
  }
}

SoftwareOutputDeviceX11::~SoftwareOutputDeviceX11() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  XFreeGC(display_, gc_);
}

void SoftwareOutputDeviceX11::EndPaint(cc::SoftwareFrameData* frame_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(canvas_);
  DCHECK(frame_data);

  if (!canvas_)
    return;

  SoftwareOutputDevice::EndPaint(frame_data);

  gfx::Rect rect = damage_rect_;
  rect.Intersect(gfx::Rect(viewport_pixel_size_));
  if (rect.IsEmpty())
    return;

  int bpp = gfx::BitsPerPixelForPixmapDepth(display_, attributes_.depth);

  if (bpp != 32 && bpp != 16 && ui::QueryRenderSupport(display_)) {
    // gfx::PutARGBImage only supports 16 and 32 bpp, but Xrender can do other
    // conversions.
    Pixmap pixmap = XCreatePixmap(
        display_, compositor_->widget(), rect.width(), rect.height(), 32);
    GC gc = XCreateGC(display_, pixmap, 0, NULL);
    XImage image;
    memset(&image, 0, sizeof(image));

    SkImageInfo info;
    size_t rowBytes;
    const void* addr = canvas_->peekPixels(&info, &rowBytes);
    image.width = viewport_pixel_size_.width();
    image.height = viewport_pixel_size_.height();
    image.depth = 32;
    image.bits_per_pixel = 32;
    image.format = ZPixmap;
    image.byte_order = LSBFirst;
    image.bitmap_unit = 8;
    image.bitmap_bit_order = LSBFirst;
    image.bytes_per_line = rowBytes;
    image.red_mask = 0xff;
    image.green_mask = 0xff00;
    image.blue_mask = 0xff0000;
    image.data = const_cast<char*>(static_cast<const char*>(addr));

    XPutImage(display_,
              pixmap,
              gc,
              &image,
              rect.x(),
              rect.y() /* source x, y */,
              0,
              0 /* dest x, y */,
              rect.width(),
              rect.height());
    XFreeGC(display_, gc);
    Picture picture = XRenderCreatePicture(
        display_, pixmap, ui::GetRenderARGB32Format(display_), 0, NULL);
    XRenderPictFormat* pictformat =
        XRenderFindVisualFormat(display_, attributes_.visual);
    Picture dest_picture = XRenderCreatePicture(
        display_, compositor_->widget(), pictformat, 0, NULL);
    XRenderComposite(display_,
                     PictOpSrc,       // op
                     picture,         // src
                     0,               // mask
                     dest_picture,    // dest
                     0,               // src_x
                     0,               // src_y
                     0,               // mask_x
                     0,               // mask_y
                     rect.x(),        // dest_x
                     rect.y(),        // dest_y
                     rect.width(),    // width
                     rect.height());  // height
    XRenderFreePicture(display_, picture);
    XRenderFreePicture(display_, dest_picture);
    XFreePixmap(display_, pixmap);
    return;
  }

  // TODO(jbauman): Switch to XShmPutImage since it's async.
  SkImageInfo info;
  size_t rowBytes;
  const void* addr = canvas_->peekPixels(&info, &rowBytes);
  gfx::PutARGBImage(display_,
                    attributes_.visual,
                    attributes_.depth,
                    compositor_->widget(),
                    gc_,
                    static_cast<const uint8*>(addr),
                    viewport_pixel_size_.width(),
                    viewport_pixel_size_.height(),
                    rect.x(),
                    rect.y(),
                    rect.x(),
                    rect.y(),
                    rect.width(),
                    rect.height());
}

}  // namespace content
