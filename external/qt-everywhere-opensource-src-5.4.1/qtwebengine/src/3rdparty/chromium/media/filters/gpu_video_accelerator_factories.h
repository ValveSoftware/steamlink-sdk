// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
#define MEDIA_FILTERS_GPU_VIDEO_ACCELERATOR_FACTORIES_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/media_export.h"

class SkBitmap;

namespace base {
class SingleThreadTaskRunner;
class SharedMemory;
}

namespace gfx {
class Rect;
class Size;
}

namespace media {

class VideoDecodeAccelerator;
class VideoEncodeAccelerator;

// Helper interface for specifying factories needed to instantiate a hardware
// video accelerator.
// Threading model:
// * The GpuVideoAcceleratorFactories may be constructed on any thread.
// * The GpuVideoAcceleratorFactories has an associated message loop, which may
//   be retrieved as |GetMessageLoop()|.
// * All calls to the Factories after construction must be made on its message
//   loop.
class MEDIA_EXPORT GpuVideoAcceleratorFactories
    : public base::RefCountedThreadSafe<GpuVideoAcceleratorFactories> {
 public:
  // Caller owns returned pointer, but should call Destroy() on it (instead of
  // directly deleting) for proper destruction, as per the
  // VideoDecodeAccelerator interface.
  virtual scoped_ptr<VideoDecodeAccelerator> CreateVideoDecodeAccelerator() = 0;

  // Caller owns returned pointer, but should call Destroy() on it (instead of
  // directly deleting) for proper destruction, as per the
  // VideoEncodeAccelerator interface.
  virtual scoped_ptr<VideoEncodeAccelerator> CreateVideoEncodeAccelerator() = 0;

  // Allocate & delete native textures.
  virtual bool CreateTextures(int32 count,
                              const gfx::Size& size,
                              std::vector<uint32>* texture_ids,
                              std::vector<gpu::Mailbox>* texture_mailboxes,
                              uint32 texture_target) = 0;
  virtual void DeleteTexture(uint32 texture_id) = 0;

  virtual void WaitSyncPoint(uint32 sync_point) = 0;

  // Read pixels within |visible_rect| boundaries from a native texture and
  // store into |pixels| as RGBA.
  virtual void ReadPixels(uint32 texture_id,
                          const gfx::Rect& visible_rect,
                          const SkBitmap& pixels) = 0;

  // Allocate & return a shared memory segment.  Caller is responsible for
  // Close()ing the returned pointer.
  virtual base::SharedMemory* CreateSharedMemory(size_t size) = 0;

  // Returns the task runner the video accelerator runs on.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() = 0;

 protected:
  friend class base::RefCountedThreadSafe<GpuVideoAcceleratorFactories>;
  virtual ~GpuVideoAcceleratorFactories();
};

}  // namespace media

#endif  // MEDIA_FILTERS_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
