// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/software_frame_manager.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/debug/alias.h"
#include "base/numerics/safe_math.h"
#include "cc/resources/shared_bitmap.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "content/public/browser/user_metrics.h"

namespace {

void ReleaseMailbox(scoped_refptr<content::SoftwareFrame> frame,
                    uint32 sync_point,
                    bool lost_resource) {}

}  // namespace

namespace content {

////////////////////////////////////////////////////////////////////////////////
// SoftwareFrame

class CONTENT_EXPORT SoftwareFrame : public base::RefCounted<SoftwareFrame> {
 private:
  friend class base::RefCounted<SoftwareFrame>;
  friend class SoftwareFrameManager;

  SoftwareFrame(base::WeakPtr<SoftwareFrameManagerClient> frame_manager_client,
                uint32 output_surface_id,
                unsigned frame_id,
                float frame_device_scale_factor,
                gfx::Size frame_size_pixels,
                scoped_ptr<cc::SharedBitmap> shared_bitmap);
  ~SoftwareFrame();

  base::WeakPtr<SoftwareFrameManagerClient> frame_manager_client_;
  const uint32 output_surface_id_;
  const unsigned frame_id_;
  float frame_device_scale_factor_;
  const gfx::Size frame_size_pixels_;
  scoped_ptr<cc::SharedBitmap> shared_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareFrame);
};

SoftwareFrame::SoftwareFrame(
    base::WeakPtr<SoftwareFrameManagerClient> frame_manager_client,
    uint32 output_surface_id,
    unsigned frame_id,
    float frame_device_scale_factor,
    gfx::Size frame_size_pixels,
    scoped_ptr<cc::SharedBitmap> shared_bitmap)
    : frame_manager_client_(frame_manager_client),
      output_surface_id_(output_surface_id),
      frame_id_(frame_id),
      frame_device_scale_factor_(frame_device_scale_factor),
      frame_size_pixels_(frame_size_pixels),
      shared_bitmap_(shared_bitmap.Pass()) {}

SoftwareFrame::~SoftwareFrame() {
  if (frame_manager_client_) {
    frame_manager_client_->SoftwareFrameWasFreed(
        output_surface_id_, frame_id_);
  }
}

////////////////////////////////////////////////////////////////////////////////
// SoftwareFrameManager

SoftwareFrameManager::SoftwareFrameManager(
    base::WeakPtr<SoftwareFrameManagerClient> client)
      : client_(client) {}

SoftwareFrameManager::~SoftwareFrameManager() {
  DiscardCurrentFrame();
}

bool SoftwareFrameManager::SwapToNewFrame(
    uint32 output_surface_id,
    const cc::SoftwareFrameData* frame_data,
    float frame_device_scale_factor,
    base::ProcessHandle process_handle) {
  scoped_ptr<cc::SharedBitmap> shared_bitmap =
      HostSharedBitmapManager::current()->GetSharedBitmapFromId(
          frame_data->size, frame_data->bitmap_id);

  if (!shared_bitmap) {
    RecordAction(
        base::UserMetricsAction("BadMessageTerminate_SharedMemoryManager1"));
    return false;
  }

  scoped_refptr<SoftwareFrame> next_frame(
      new SoftwareFrame(client_,
                        output_surface_id,
                        frame_data->id,
                        frame_device_scale_factor,
                        frame_data->size,
                        shared_bitmap.Pass()));
  current_frame_.swap(next_frame);
  return true;
}

bool SoftwareFrameManager::HasCurrentFrame() const {
  return current_frame_.get() ? true : false;
}

void SoftwareFrameManager::DiscardCurrentFrame() {
  if (!HasCurrentFrame())
    return;
  current_frame_ = NULL;
  RendererFrameManager::GetInstance()->RemoveFrame(this);
}

void SoftwareFrameManager::SwapToNewFrameComplete(bool visible) {
  DCHECK(HasCurrentFrame());
  RendererFrameManager::GetInstance()->AddFrame(this, visible);
}

void SoftwareFrameManager::SetVisibility(bool visible) {
  if (HasCurrentFrame()) {
    if (visible) {
      RendererFrameManager::GetInstance()->LockFrame(this);
    } else {
      RendererFrameManager::GetInstance()->UnlockFrame(this);
    }
  }
}

uint32 SoftwareFrameManager::GetCurrentFrameOutputSurfaceId() const {
  DCHECK(HasCurrentFrame());
  return current_frame_->output_surface_id_;
}

void SoftwareFrameManager::GetCurrentFrameMailbox(
    cc::TextureMailbox* mailbox,
    scoped_ptr<cc::SingleReleaseCallback>* callback) {
  DCHECK(HasCurrentFrame());
  *mailbox = cc::TextureMailbox(current_frame_->shared_bitmap_->memory(),
                                current_frame_->frame_size_pixels_);
  *callback = cc::SingleReleaseCallback::Create(
      base::Bind(ReleaseMailbox, current_frame_));
}

void* SoftwareFrameManager::GetCurrentFramePixels() const {
  DCHECK(HasCurrentFrame());
  DCHECK(current_frame_->shared_bitmap_);
  return current_frame_->shared_bitmap_->pixels();
}

float SoftwareFrameManager::GetCurrentFrameDeviceScaleFactor() const {
  DCHECK(HasCurrentFrame());
  return current_frame_->frame_device_scale_factor_;
}

gfx::Size SoftwareFrameManager::GetCurrentFrameSizeInPixels() const {
  DCHECK(HasCurrentFrame());
  return current_frame_->frame_size_pixels_;
}

gfx::Size SoftwareFrameManager::GetCurrentFrameSizeInDIP() const {
  DCHECK(HasCurrentFrame());
  return ConvertSizeToDIP(current_frame_->frame_device_scale_factor_,
                          current_frame_->frame_size_pixels_);
}

void SoftwareFrameManager::EvictCurrentFrame() {
  DCHECK(HasCurrentFrame());
  DiscardCurrentFrame();
  if (client_)
    client_->ReleaseReferencesToSoftwareFrame();
}

}  // namespace content
