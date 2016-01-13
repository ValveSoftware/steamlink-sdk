// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport.h"

// Stub to build on platforms we don't fully support yet.

namespace mojo {
namespace services {

class NativeViewportStub : public NativeViewport {
 public:
  NativeViewportStub(NativeViewportDelegate* delegate)
      : delegate_(delegate) {
  }
  virtual ~NativeViewportStub() {
  }

 private:
  // Overridden from NativeViewport:
  virtual void Init() OVERRIDE {
  }
  virtual void Show() OVERRIDE {
  }
  virtual void Hide() OVERRIDE {
  }
  virtual void Close() OVERRIDE {
    delegate_->OnDestroyed();
  }
  virtual gfx::Size GetSize() OVERRIDE {
    return gfx::Size();
  }
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE {
  }

  NativeViewportDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportStub);
};

// static
scoped_ptr<NativeViewport> NativeViewport::Create(
    shell::Context* context,
    NativeViewportDelegate* delegate) {
  return scoped_ptr<NativeViewport>(new NativeViewportStub(delegate)).Pass();
}

}  // namespace services
}  // namespace mojo
