// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/compositor_software_output_device.h"

#include "base/logging.h"
#include "cc/output/software_frame_data.h"
#include "content/child/child_shared_bitmap_manager.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/skia_util.h"

namespace content {

CompositorSoftwareOutputDevice::Buffer::Buffer(
    unsigned id,
    scoped_ptr<cc::SharedBitmap> bitmap)
    : id_(id), shared_bitmap_(bitmap.Pass()), free_(true), parent_(NULL) {}

CompositorSoftwareOutputDevice::Buffer::~Buffer() {
}

void CompositorSoftwareOutputDevice::Buffer::SetParent(
    Buffer* parent, const gfx::Rect& damage) {
  parent_ = parent;
  damage_ = damage;
}

bool CompositorSoftwareOutputDevice::Buffer::FindDamageDifferenceFrom(
    Buffer* buffer, SkRegion* result) const {
  if (!buffer)
    return false;

  if (buffer == this) {
    *result = SkRegion();
    return true;
  }

  SkRegion damage;
  const Buffer* current = this;
  while (current->parent_) {
    damage.op(RectToSkIRect(current->damage_), SkRegion::kUnion_Op);
    if (current->parent_ == buffer) {
      *result = damage;
      return true;
    }
    current = current->parent_;
  }

  return false;
}

CompositorSoftwareOutputDevice::CompositorSoftwareOutputDevice()
    : current_index_(-1),
      next_buffer_id_(1),
      shared_bitmap_manager_(
          RenderThreadImpl::current()->shared_bitmap_manager()) {
  DetachFromThread();
}

CompositorSoftwareOutputDevice::~CompositorSoftwareOutputDevice() {
  DCHECK(CalledOnValidThread());
}

unsigned CompositorSoftwareOutputDevice::GetNextId() {
  unsigned id = next_buffer_id_++;
  // Zero is reserved to label invalid frame id.
  if (id == 0)
    id = next_buffer_id_++;
  return id;
}

CompositorSoftwareOutputDevice::Buffer*
CompositorSoftwareOutputDevice::CreateBuffer() {
  scoped_ptr<cc::SharedBitmap> shared_bitmap =
      shared_bitmap_manager_->AllocateSharedBitmap(viewport_pixel_size_);
  CHECK(shared_bitmap);
  return new Buffer(GetNextId(), shared_bitmap.Pass());
}

size_t CompositorSoftwareOutputDevice::FindFreeBuffer(size_t hint) {
  for (size_t i = 0; i < buffers_.size(); ++i) {
    size_t index = (hint + i) % buffers_.size();
    if (buffers_[index]->free())
      return index;
  }

  buffers_.push_back(CreateBuffer());
  return buffers_.size() - 1;
}

void CompositorSoftwareOutputDevice::Resize(
    const gfx::Size& viewport_pixel_size,
    float scale_factor) {
  DCHECK(CalledOnValidThread());

  scale_factor_ = scale_factor;

  if (viewport_pixel_size_ == viewport_pixel_size)
    return;

  // Keep non-ACKed buffers in awaiting_ack_ until they get acknowledged.
  for (size_t i = 0; i < buffers_.size(); ++i) {
    if (!buffers_[i]->free()) {
      awaiting_ack_.push_back(buffers_[i]);
      buffers_[i] = NULL;
    }
  }

  buffers_.clear();
  current_index_ = -1;
  viewport_pixel_size_ = viewport_pixel_size;
}

void CompositorSoftwareOutputDevice::DiscardBackbuffer() {
  // Keep non-ACKed buffers in awaiting_ack_ until they get acknowledged.
  for (size_t i = 0; i < buffers_.size(); ++i) {
    if (!buffers_[i]->free()) {
      awaiting_ack_.push_back(buffers_[i]);
      buffers_[i] = NULL;
    }
  }
  buffers_.clear();
  current_index_ = -1;
}

void CompositorSoftwareOutputDevice::EnsureBackbuffer() {
}

SkCanvas* CompositorSoftwareOutputDevice::BeginPaint(
    const gfx::Rect& damage_rect) {
  DCHECK(CalledOnValidThread());

  Buffer* previous = NULL;
  if (current_index_ != size_t(-1))
    previous = buffers_[current_index_];
  current_index_ = FindFreeBuffer(current_index_ + 1);
  Buffer* current = buffers_[current_index_];
  DCHECK(current->free());
  current->SetFree(false);

  // Set up a canvas for the current front buffer.
  SkImageInfo info = SkImageInfo::MakeN32Premul(viewport_pixel_size_.width(),
                                                viewport_pixel_size_.height());
  SkBitmap bitmap;
  bitmap.installPixels(info, current->memory(), info.minRowBytes());
  canvas_ = skia::AdoptRef(new SkCanvas(bitmap));

  if (!previous) {
    DCHECK(damage_rect == gfx::Rect(viewport_pixel_size_));
  } else {
    // Find the smallest damage region that needs
    // to be copied from the |previous| buffer.
    SkRegion region;
    bool found =
        current->FindDamageDifferenceFrom(previous, &region) ||
        previous->FindDamageDifferenceFrom(current, &region);
    if (!found)
      region = SkRegion(RectToSkIRect(gfx::Rect(viewport_pixel_size_)));
    region.op(RectToSkIRect(damage_rect), SkRegion::kDifference_Op);

    // Copy over the damage region.
    if (!region.isEmpty()) {
      SkBitmap back_bitmap;
      back_bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                            viewport_pixel_size_.width(),
                            viewport_pixel_size_.height());
      back_bitmap.setPixels(previous->memory());

      for (SkRegion::Iterator it(region); !it.done(); it.next()) {
        const SkIRect& src_rect = it.rect();
        SkRect dst_rect = SkRect::Make(src_rect);
        canvas_->drawBitmapRect(back_bitmap, &src_rect, dst_rect, NULL);
      }
    }
  }

  // Make |current| child of |previous| and orphan all of |current|'s children.
  current->SetParent(previous, damage_rect);
  for (size_t i = 0; i < buffers_.size(); ++i) {
    Buffer* buffer = buffers_[i];
    if (buffer->parent() == current)
      buffer->SetParent(NULL, gfx::Rect(viewport_pixel_size_));
  }
  damage_rect_ = damage_rect;

  return canvas_.get();
}

void CompositorSoftwareOutputDevice::EndPaint(
    cc::SoftwareFrameData* frame_data) {
  DCHECK(CalledOnValidThread());
  DCHECK(frame_data);

  Buffer* buffer = buffers_[current_index_];
  frame_data->id = buffer->id();
  frame_data->size = viewport_pixel_size_;
  frame_data->damage_rect = damage_rect_;
  frame_data->bitmap_id = buffer->shared_bitmap_id();
}

void CompositorSoftwareOutputDevice::ReclaimSoftwareFrame(unsigned id) {
  DCHECK(CalledOnValidThread());

  if (!id)
    return;

  // The reclaimed buffer id might not be among the currently
  // active buffers if we got a resize event in the mean time.
  ScopedVector<Buffer>::iterator it =
      std::find_if(buffers_.begin(), buffers_.end(), CompareById(id));
  if (it != buffers_.end()) {
    DCHECK(!(*it)->free());
    (*it)->SetFree(true);
    return;
  } else {
    it = std::find_if(awaiting_ack_.begin(), awaiting_ack_.end(),
                      CompareById(id));
    DCHECK(it != awaiting_ack_.end());
    awaiting_ack_.erase(it);
  }
}

}  // namespace content
