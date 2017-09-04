// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_mojo_handle_attachment.h"

#include <utility>

#include "build/build_config.h"

namespace IPC {
namespace internal {

MojoHandleAttachment::MojoHandleAttachment(mojo::ScopedHandle handle)
    : handle_(std::move(handle)) {}

MojoHandleAttachment::~MojoHandleAttachment() {
}

MessageAttachment::Type MojoHandleAttachment::GetType() const {
  return TYPE_MOJO_HANDLE;
}

#if defined(OS_POSIX)
base::PlatformFile MojoHandleAttachment::TakePlatformFile() {
  NOTREACHED();
  return base::kInvalidPlatformFile;
}
#endif  // OS_POSIX

mojo::ScopedHandle MojoHandleAttachment::TakeHandle() {
  return std::move(handle_);
}

}  // namespace internal
}  // namespace IPC
