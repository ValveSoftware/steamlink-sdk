// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_WINDOW_ACTIVITY_TRACKER_AURA_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_WINDOW_ACTIVITY_TRACKER_AURA_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/media/capture/window_activity_tracker.h"
#include "content/common/content_export.h"
#include "ui/aura/window.h"
#include "ui/events/event_handler.h"

namespace content {

// Tracks UI events and makes a decision on whether the user has been
// actively interacting with a specified window.
class CONTENT_EXPORT WindowActivityTrackerAura : public WindowActivityTracker,
                                                 public ui::EventHandler,
                                                 public aura::WindowObserver {
 public:
  explicit WindowActivityTrackerAura(aura::Window* window);
  ~WindowActivityTrackerAura() final;

  base::WeakPtr<WindowActivityTracker> GetWeakPtr() final;

 private:
  // ui::EventHandler overrides.
  void OnEvent(ui::Event* event) final;

  // aura::WindowObserver overrides.
  void OnWindowDestroying(aura::Window* window) final;

  aura::Window* window_;

  base::WeakPtrFactory<WindowActivityTrackerAura> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowActivityTrackerAura);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_WINDOW_ACTIVITY_TRACKER_AURA_H_
