// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDERER_FRAME_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDERER_FRAME_MANAGER_H_

#include <list>
#include <map>

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT RendererFrameManagerClient {
 public:
  virtual ~RendererFrameManagerClient() {}
  virtual void EvictCurrentFrame() = 0;
};

class CONTENT_EXPORT RendererFrameManager {
 public:
  static RendererFrameManager* GetInstance();

  void AddFrame(RendererFrameManagerClient*, bool locked);
  void RemoveFrame(RendererFrameManagerClient*);
  void LockFrame(RendererFrameManagerClient*);
  void UnlockFrame(RendererFrameManagerClient*);

  size_t max_number_of_saved_frames() const {
    return max_number_of_saved_frames_;
  }

  // For testing only
  void set_max_handles(float max_handles) { max_handles_ = max_handles; }

 private:
  RendererFrameManager();
  ~RendererFrameManager();
  void CullUnlockedFrames();

  friend struct DefaultSingletonTraits<RendererFrameManager>;

  std::map<RendererFrameManagerClient*, size_t> locked_frames_;
  std::list<RendererFrameManagerClient*> unlocked_frames_;
  size_t max_number_of_saved_frames_;
  float max_handles_;

  DISALLOW_COPY_AND_ASSIGN(RendererFrameManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDERER_FRAME_MANAGER_H_
