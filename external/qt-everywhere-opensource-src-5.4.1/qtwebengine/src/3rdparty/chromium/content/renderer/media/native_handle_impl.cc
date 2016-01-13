// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/native_handle_impl.h"

namespace content {

NativeHandleImpl::NativeHandleImpl(scoped_refptr<media::VideoFrame> frame)
    : frame_(frame) {}

NativeHandleImpl::~NativeHandleImpl() {}

void* NativeHandleImpl::GetHandle() { return frame_.get(); }

}  // namespace content
