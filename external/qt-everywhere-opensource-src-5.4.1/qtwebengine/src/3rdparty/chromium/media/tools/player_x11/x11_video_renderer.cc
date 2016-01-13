// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/tools/player_x11/x11_video_renderer.h"

#include <dlfcn.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xcomposite.h>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "media/base/video_frame.h"
#include "media/base/yuv_convert.h"

// Creates a 32-bit XImage.
static XImage* CreateImage(Display* display, int width, int height) {
  VLOG(0) << "Allocating XImage " << width << "x" << height;
  return  XCreateImage(display,
                       DefaultVisual(display, DefaultScreen(display)),
                       DefaultDepth(display, DefaultScreen(display)),
                       ZPixmap,
                       0,
                       static_cast<char*>(malloc(width * height * 4)),
                       width,
                       height,
                       32,
                       width * 4);
}

// Returns the picture format for ARGB.
// This method is originally from chrome/common/x11_util.cc.
static XRenderPictFormat* GetRenderARGB32Format(Display* dpy) {
  static XRenderPictFormat* pictformat = NULL;
  if (pictformat)
    return pictformat;

  // First look for a 32-bit format which ignores the alpha value.
  XRenderPictFormat templ;
  templ.depth = 32;
  templ.type = PictTypeDirect;
  templ.direct.red = 16;
  templ.direct.green = 8;
  templ.direct.blue = 0;
  templ.direct.redMask = 0xff;
  templ.direct.greenMask = 0xff;
  templ.direct.blueMask = 0xff;
  templ.direct.alphaMask = 0;

  static const unsigned long kMask =
      PictFormatType | PictFormatDepth |
      PictFormatRed | PictFormatRedMask |
      PictFormatGreen | PictFormatGreenMask |
      PictFormatBlue | PictFormatBlueMask |
      PictFormatAlphaMask;

  pictformat = XRenderFindFormat(dpy, kMask, &templ, 0 /* first result */);

  if (!pictformat) {
    // Not all X servers support xRGB32 formats. However, the XRender spec
    // says that they must support an ARGB32 format, so we can always return
    // that.
    pictformat = XRenderFindStandardFormat(dpy, PictStandardARGB32);
    CHECK(pictformat) << "XRender ARGB32 not supported.";
  }

  return pictformat;
}

X11VideoRenderer::X11VideoRenderer(Display* display, Window window)
    : display_(display),
      window_(window),
      image_(NULL),
      picture_(0),
      use_render_(false) {
}

X11VideoRenderer::~X11VideoRenderer() {
  if (image_)
    XDestroyImage(image_);
  if (use_render_)
    XRenderFreePicture(display_, picture_);
}

void X11VideoRenderer::Paint(
    const scoped_refptr<media::VideoFrame>& video_frame) {
  if (!image_)
    Initialize(video_frame->coded_size(), video_frame->visible_rect());

  const int coded_width = video_frame->coded_size().width();
  const int coded_height = video_frame->coded_size().height();
  const int visible_width = video_frame->visible_rect().width();
  const int visible_height = video_frame->visible_rect().height();

  // Check if we need to reallocate our XImage.
  if (image_->width != coded_width || image_->height != coded_height) {
    XDestroyImage(image_);
    image_ = CreateImage(display_, coded_width, coded_height);
  }

  // Convert YUV frame to RGB.
  DCHECK(video_frame->format() == media::VideoFrame::YV12 ||
         video_frame->format() == media::VideoFrame::I420 ||
         video_frame->format() == media::VideoFrame::YV16);
  DCHECK(video_frame->stride(media::VideoFrame::kUPlane) ==
         video_frame->stride(media::VideoFrame::kVPlane));

  DCHECK(image_->data);
  media::YUVType yuv_type = (video_frame->format() == media::VideoFrame::YV12 ||
                             video_frame->format() == media::VideoFrame::I420)
                                ? media::YV12
                                : media::YV16;
  media::ConvertYUVToRGB32(video_frame->data(media::VideoFrame::kYPlane),
                           video_frame->data(media::VideoFrame::kUPlane),
                           video_frame->data(media::VideoFrame::kVPlane),
                           (uint8*)image_->data, coded_width, coded_height,
                           video_frame->stride(media::VideoFrame::kYPlane),
                           video_frame->stride(media::VideoFrame::kUPlane),
                           image_->bytes_per_line,
                           yuv_type);

  if (use_render_) {
    // If XRender is used, we'll upload the image to a pixmap. And then
    // creats a picture from the pixmap and composite the picture over
    // the picture represending the window.

    // Creates a XImage.
    XImage image;
    memset(&image, 0, sizeof(image));
    image.width = coded_width;
    image.height = coded_height;
    image.depth = 32;
    image.bits_per_pixel = 32;
    image.format = ZPixmap;
    image.byte_order = LSBFirst;
    image.bitmap_unit = 8;
    image.bitmap_bit_order = LSBFirst;
    image.bytes_per_line = image_->bytes_per_line;
    image.red_mask = 0xff;
    image.green_mask = 0xff00;
    image.blue_mask = 0xff0000;
    image.data = image_->data;

    // Creates a pixmap and uploads from the XImage.
    unsigned long pixmap = XCreatePixmap(display_, window_,
                                         visible_width, visible_height,
                                         32);
    GC gc = XCreateGC(display_, pixmap, 0, NULL);
    XPutImage(display_, pixmap, gc, &image,
              video_frame->visible_rect().x(),
              video_frame->visible_rect().y(),
              0, 0,
              visible_width, visible_height);
    XFreeGC(display_, gc);

    // Creates the picture representing the pixmap.
    unsigned long picture = XRenderCreatePicture(
        display_, pixmap, GetRenderARGB32Format(display_), 0, NULL);

    // Composite the picture over the picture representing the window.
    XRenderComposite(display_, PictOpSrc, picture, 0,
                     picture_, 0, 0, 0, 0, 0, 0,
                     visible_width, visible_height);

    XRenderFreePicture(display_, picture);
    XFreePixmap(display_, pixmap);
    return;
  }

  // If XRender is not used, simply put the image to the server.
  // This will have a tearing effect but this is OK.
  // TODO(hclam): Upload the image to a pixmap and do XCopyArea()
  // to the window.
  GC gc = XCreateGC(display_, window_, 0, NULL);
  XPutImage(display_, window_, gc, image_,
            video_frame->visible_rect().x(),
            video_frame->visible_rect().y(),
            0, 0, visible_width, visible_height);
  XFlush(display_);
  XFreeGC(display_, gc);
}

void X11VideoRenderer::Initialize(gfx::Size coded_size,
                                  gfx::Rect visible_rect) {
  CHECK(!image_);
  VLOG(0) << "Initializing X11 Renderer...";

  // Resize the window to fit that of the video.
  XResizeWindow(display_, window_, visible_rect.width(), visible_rect.height());
  image_ = CreateImage(display_, coded_size.width(), coded_size.height());

  // Testing XRender support. We'll use the very basic of XRender
  // so if it presents it is already good enough. We don't need
  // to check its version.
  int dummy;
  use_render_ = XRenderQueryExtension(display_, &dummy, &dummy);

  if (use_render_) {
    VLOG(0) << "Using XRender extension.";

    // If we are using XRender, we'll create a picture representing the
    // window.
    XWindowAttributes attr;
    XGetWindowAttributes(display_, window_, &attr);

    XRenderPictFormat* pictformat = XRenderFindVisualFormat(
        display_,
        attr.visual);
    CHECK(pictformat) << "XRender does not support default visual";

    picture_ = XRenderCreatePicture(display_, window_, pictformat, 0, NULL);
    CHECK(picture_) << "Backing picture not created";
  }
}
