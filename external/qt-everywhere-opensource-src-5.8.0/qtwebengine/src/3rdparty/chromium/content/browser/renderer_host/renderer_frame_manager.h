// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDERER_FRAME_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDERER_FRAME_MANAGER_H_

#include <stddef.h>

#include <list>
#include <map>

#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/singleton.h"
#include "content/common/content_export.h"

namespace content {

class RenderWidgetHostViewAuraTest;

class CONTENT_EXPORT RendererFrameManagerClient {
 public:
  virtual ~RendererFrameManagerClient() {}
  virtual void EvictCurrentFrame() = 0;
};

// This class is responsible for globally managing which renderers keep their
// compositor frame when offscreen. We actively discard compositor frames for
// offscreen tabs, but keep a minimum amount, as an LRU cache, to make switching
// between a small set of tabs faster. The limit is a soft limit, because
// clients can lock their frame to prevent it from being discarded, e.g. if the
// tab is visible, or while capturing a screenshot.
class CONTENT_EXPORT RendererFrameManager {
 public:
  static RendererFrameManager* GetInstance();

  void AddFrame(RendererFrameManagerClient*, bool locked);
  void RemoveFrame(RendererFrameManagerClient*);
  void LockFrame(RendererFrameManagerClient*);
  void UnlockFrame(RendererFrameManagerClient*);

  size_t GetMaxNumberOfSavedFrames() const;

  // For testing only
  void set_max_number_of_saved_frames(size_t max_number_of_saved_frames) {
    max_number_of_saved_frames_ = max_number_of_saved_frames;
  }
  void set_max_handles(float max_handles) { max_handles_ = max_handles; }

 private:
  // Please remove when crbug.com/443824 has been fixed.
  friend class RenderWidgetHostViewAuraTest;

  RendererFrameManager();
  ~RendererFrameManager();
  void CullUnlockedFrames(size_t saved_frame_limit);

  // React on memory pressure events to adjust the number of cached frames.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  friend struct base::DefaultSingletonTraits<RendererFrameManager>;

  // Listens for system under pressure notifications and adjusts number of
  // cached frames accordingly.
  base::MemoryPressureListener memory_pressure_listener_;

  std::map<RendererFrameManagerClient*, size_t> locked_frames_;
  std::list<RendererFrameManagerClient*> unlocked_frames_;
  size_t max_number_of_saved_frames_;
  float max_handles_;

  DISALLOW_COPY_AND_ASSIGN(RendererFrameManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDERER_FRAME_MANAGER_H_
