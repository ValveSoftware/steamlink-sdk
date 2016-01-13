// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_GLES2_MOJO_BUFFER_BACKING_H_
#define MOJO_SERVICES_GLES2_MOJO_BUFFER_BACKING_H_

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/common/buffer.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {
namespace gles2 {

class MojoBufferBacking : public gpu::BufferBacking {
 public:
  MojoBufferBacking(mojo::ScopedSharedBufferHandle handle,
                    void* memory,
                    size_t size);
  virtual ~MojoBufferBacking();

  static scoped_ptr<gpu::BufferBacking> Create(
      mojo::ScopedSharedBufferHandle handle,
      size_t size);

  virtual void* GetMemory() const OVERRIDE;
  virtual size_t GetSize() const OVERRIDE;

 private:
  mojo::ScopedSharedBufferHandle handle_;
  void* memory_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(MojoBufferBacking);
};

}  // namespace gles2
}  // namespace mojo

#endif  // MOJO_SERVICES_GLES2_MOJO_BUFFER_BACKING_H_
