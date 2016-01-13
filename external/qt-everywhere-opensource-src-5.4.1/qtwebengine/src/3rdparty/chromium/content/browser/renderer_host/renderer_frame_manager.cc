// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/renderer_frame_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/sys_info.h"
#include "content/common/host_shared_bitmap_manager.h"

namespace content {

RendererFrameManager* RendererFrameManager::GetInstance() {
  return Singleton<RendererFrameManager>::get();
}

void RendererFrameManager::AddFrame(RendererFrameManagerClient* frame,
                                    bool locked) {
  RemoveFrame(frame);
  if (locked)
    locked_frames_[frame] = 1;
  else
    unlocked_frames_.push_front(frame);
  CullUnlockedFrames();
}

void RendererFrameManager::RemoveFrame(RendererFrameManagerClient* frame) {
  std::map<RendererFrameManagerClient*, size_t>::iterator locked_iter =
      locked_frames_.find(frame);
  if (locked_iter != locked_frames_.end())
    locked_frames_.erase(locked_iter);
  unlocked_frames_.remove(frame);
}

void RendererFrameManager::LockFrame(RendererFrameManagerClient* frame) {
  std::list<RendererFrameManagerClient*>::iterator unlocked_iter =
    std::find(unlocked_frames_.begin(), unlocked_frames_.end(), frame);
  if (unlocked_iter != unlocked_frames_.end()) {
    DCHECK(locked_frames_.find(frame) == locked_frames_.end());
    unlocked_frames_.remove(frame);
    locked_frames_[frame] = 1;
  } else {
    DCHECK(locked_frames_.find(frame) != locked_frames_.end());
    locked_frames_[frame]++;
  }
}

void RendererFrameManager::UnlockFrame(RendererFrameManagerClient* frame) {
  DCHECK(locked_frames_.find(frame) != locked_frames_.end());
  size_t locked_count = locked_frames_[frame];
  DCHECK(locked_count);
  if (locked_count > 1) {
    locked_frames_[frame]--;
  } else {
    RemoveFrame(frame);
    unlocked_frames_.push_front(frame);
    CullUnlockedFrames();
  }
}

RendererFrameManager::RendererFrameManager() {
  max_number_of_saved_frames_ =
#if defined(OS_ANDROID)
      1;
#else
      std::min(5, 2 + (base::SysInfo::AmountOfPhysicalMemoryMB() / 256));
#endif
  max_handles_ = base::SharedMemory::GetHandleLimit() / 8.0f;
}

RendererFrameManager::~RendererFrameManager() {}

void RendererFrameManager::CullUnlockedFrames() {
  uint32 saved_frame_limit = max_number_of_saved_frames();

  if (unlocked_frames_.size() + locked_frames_.size() > 0) {
    float handles_per_frame =
        HostSharedBitmapManager::current()->AllocatedBitmapCount() * 1.0f /
        (unlocked_frames_.size() + locked_frames_.size());

    saved_frame_limit = std::max(
        1,
        static_cast<int>(std::min(static_cast<float>(saved_frame_limit),
                                  max_handles_ / handles_per_frame)));
  }
  while (!unlocked_frames_.empty() &&
         unlocked_frames_.size() + locked_frames_.size() > saved_frame_limit) {
    size_t old_size = unlocked_frames_.size();
    // Should remove self from list.
    unlocked_frames_.back()->EvictCurrentFrame();
    DCHECK_EQ(unlocked_frames_.size() + 1, old_size);
  }
}

}  // namespace content
