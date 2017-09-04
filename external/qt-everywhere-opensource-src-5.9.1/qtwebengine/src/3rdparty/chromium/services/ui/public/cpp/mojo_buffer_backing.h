// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_MOJO_BUFFER_BACKING_H_
#define SERVICES_UI_PUBLIC_CPP_MOJO_BUFFER_BACKING_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "gpu/command_buffer/common/buffer.h"
#include "mojo/public/cpp/system/core.h"

namespace ui {

class MojoBufferBacking : public gpu::BufferBacking {
 public:
  MojoBufferBacking(mojo::ScopedSharedBufferMapping mapping, size_t size);
  ~MojoBufferBacking() override;

  static std::unique_ptr<gpu::BufferBacking> Create(
      mojo::ScopedSharedBufferHandle handle,
      size_t size);

  void* GetMemory() const override;
  size_t GetSize() const override;

 private:
  mojo::ScopedSharedBufferMapping mapping_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(MojoBufferBacking);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_MOJO_BUFFER_BACKING_H_
