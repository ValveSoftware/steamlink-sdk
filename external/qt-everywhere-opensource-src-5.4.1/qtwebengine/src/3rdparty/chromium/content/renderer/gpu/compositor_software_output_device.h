// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/shared_memory.h"
#include "base/threading/non_thread_safe.h"
#include "cc/output/software_output_device.h"
#include "cc/resources/shared_bitmap.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"

class SkRegion;

namespace cc {
class SharedBitmapManager;
}

namespace content {

// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when BindToClient is called.
class CompositorSoftwareOutputDevice
    : NON_EXPORTED_BASE(public cc::SoftwareOutputDevice),
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
public:
  CompositorSoftwareOutputDevice();
  virtual ~CompositorSoftwareOutputDevice();

  virtual void Resize(const gfx::Size& pixel_size, float scale_factor) OVERRIDE;

  virtual SkCanvas* BeginPaint(const gfx::Rect& damage_rect) OVERRIDE;
  virtual void EndPaint(cc::SoftwareFrameData* frame_data) OVERRIDE;
  virtual void EnsureBackbuffer() OVERRIDE;
  virtual void DiscardBackbuffer() OVERRIDE;

  virtual void ReclaimSoftwareFrame(unsigned id) OVERRIDE;

private:
  // Internal buffer class that manages shared memory lifetime and ownership.
  // It also tracks buffers' history so we can calculate what's the minimum
  // damage rect difference between any two given buffers (see SetParent and
  // FindDamageDifferenceFrom).
  class Buffer {
   public:
    explicit Buffer(unsigned id, scoped_ptr<cc::SharedBitmap> bitmap);
    ~Buffer();

    unsigned id() const { return id_; }

    void* memory() const { return shared_bitmap_->pixels(); }
    cc::SharedBitmapId shared_bitmap_id() const { return shared_bitmap_->id(); }

    bool free() const { return free_; }
    void SetFree(bool free) { free_ = free; }

    Buffer* parent() const { return parent_; }
    void SetParent(Buffer* parent, const gfx::Rect& damage);

    bool FindDamageDifferenceFrom(Buffer* buffer, SkRegion* result) const;

   private:
    const unsigned id_;
    scoped_ptr<cc::SharedBitmap> shared_bitmap_;
    bool free_;
    Buffer* parent_;
    gfx::Rect damage_;

    DISALLOW_COPY_AND_ASSIGN(Buffer);
  };

  class CompareById {
   public:
    CompareById(unsigned id) : id_(id) {}

    bool operator()(const Buffer* buffer) const {
      return buffer->id() == id_;
    }

   private:
    const unsigned id_;
  };

  unsigned GetNextId();
  Buffer* CreateBuffer();
  size_t FindFreeBuffer(size_t hint);

  size_t current_index_;
  unsigned next_buffer_id_;
  ScopedVector<Buffer> buffers_;
  ScopedVector<Buffer> awaiting_ack_;
  cc::SharedBitmapManager* shared_bitmap_manager_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_H_
