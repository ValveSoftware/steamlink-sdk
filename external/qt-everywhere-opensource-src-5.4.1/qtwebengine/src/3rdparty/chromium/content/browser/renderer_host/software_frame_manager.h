// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SOFTWARE_FRAME_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_SOFTWARE_FRAME_MANAGER_H_

#include <list>
#include <set>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "cc/output/software_frame_data.h"
#include "cc/resources/single_release_callback.h"
#include "cc/resources/texture_mailbox.h"
#include "content/browser/renderer_host/renderer_frame_manager.h"
#include "content/common/content_export.h"
#include "ui/gfx/size.h"

namespace content {
class SoftwareFrame;

class CONTENT_EXPORT SoftwareFrameManagerClient {
 public:
  // Called when the memory for the current software frame was freed.
  virtual void SoftwareFrameWasFreed(
      uint32 output_surface_id, unsigned frame_id) = 0;

  // Called when the SoftwareFrameMemoryManager has requested that the frame
  // be evicted. Upon receiving this callback, the client should release any
  // references that it may hold to the current frame, to ensure that its memory
  // is freed expediently.
  virtual void ReleaseReferencesToSoftwareFrame() = 0;
};

class CONTENT_EXPORT SoftwareFrameManager : public RendererFrameManagerClient {
 public:
  explicit SoftwareFrameManager(
      base::WeakPtr<SoftwareFrameManagerClient> client);
  virtual ~SoftwareFrameManager();

  // Swaps to a new frame from shared memory. This frame is guaranteed to
  // not be evicted until SwapToNewFrameComplete is called.
  bool SwapToNewFrame(
      uint32 output_surface_id,
      const cc::SoftwareFrameData* frame_data,
      float frame_device_scale_factor,
      base::ProcessHandle process_handle);
  void SwapToNewFrameComplete(bool visible);
  void SetVisibility(bool visible);
  bool HasCurrentFrame() const;
  void DiscardCurrentFrame();
  uint32 GetCurrentFrameOutputSurfaceId() const;
  void GetCurrentFrameMailbox(
      cc::TextureMailbox* mailbox,
      scoped_ptr<cc::SingleReleaseCallback>* callback);
  void* GetCurrentFramePixels() const;
  float GetCurrentFrameDeviceScaleFactor() const;
  gfx::Size GetCurrentFrameSizeInPixels() const;
  gfx::Size GetCurrentFrameSizeInDIP() const;

 private:
  // Called by SoftwareFrameMemoryManager to demand that the current frame
  // be evicted.
  virtual void EvictCurrentFrame() OVERRIDE;

  base::WeakPtr<SoftwareFrameManagerClient> client_;

  // This holds the current software framebuffer.
  scoped_refptr<SoftwareFrame> current_frame_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareFrameManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SOFTWARE_FRAME_MANAGER_H_
