// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport.h"

#import <AppKit/NSApplication.h>
#import <AppKit/NSView.h>
#import <AppKit/NSWindow.h>

#include "base/bind.h"
#include "ui/gfx/rect.h"

namespace mojo {
namespace services {

class NativeViewportMac : public NativeViewport {
 public:
  NativeViewportMac(NativeViewportDelegate* delegate)
      : delegate_(delegate),
        window_(nil) {
  }

  virtual ~NativeViewportMac() {
    [window_ orderOut:nil];
    [window_ close];
  }

 private:
  // Overridden from NativeViewport:
  virtual void Init(const gfx::Rect& bounds) OVERRIDE {
    [NSApplication sharedApplication];

    rect_ = bounds;
    window_ = [[NSWindow alloc]
                  initWithContentRect:NSRectFromCGRect(rect_.ToCGRect())
                            styleMask:NSTitledWindowMask
                              backing:NSBackingStoreBuffered
                                defer:NO];
    delegate_->OnAcceleratedWidgetAvailable([window_ contentView]);
    delegate_->OnBoundsChanged(rect_);
  }

  virtual void Show() OVERRIDE {
    [window_ orderFront:nil];
  }

  virtual void Hide() OVERRIDE {
    [window_ orderOut:nil];
  }

  virtual void Close() OVERRIDE {
    // TODO(beng): perform this in response to NSWindow destruction.
    delegate_->OnDestroyed();
  }

  virtual gfx::Size GetSize() OVERRIDE {
    return rect_.size();
  }

  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void SetCapture() OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void ReleaseCapture() OVERRIDE {
    NOTIMPLEMENTED();
  }

  NativeViewportDelegate* delegate_;
  NSWindow* window_;
  gfx::Rect rect_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportMac);
};

// static
scoped_ptr<NativeViewport> NativeViewport::Create(
    shell::Context* context,
    NativeViewportDelegate* delegate) {
  return scoped_ptr<NativeViewport>(new NativeViewportMac(delegate)).Pass();
}

}  // namespace services
}  // namespace mojo
