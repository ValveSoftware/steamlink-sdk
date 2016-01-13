// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_CAPTURE_CONTROLLER_H_
#define UI_WM_CORE_CAPTURE_CONTROLLER_H_

#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/wm_export.h"

namespace wm {

// Internal CaptureClient implementation. See ScopedCaptureClient for details.
class WM_EXPORT CaptureController : public aura::client::CaptureClient {
 public:
  // Adds |root| to the list of RootWindows notified when capture changes.
  void Attach(aura::Window* root);

  // Removes |root| from the list of RootWindows notified when capture changes.
  void Detach(aura::Window* root);

  // Returns true if this CaptureController is installed on at least one
  // RootWindow.
  bool is_active() const { return !root_windows_.empty(); }

  // Overridden from aura::client::CaptureClient:
  virtual void SetCapture(aura::Window* window) OVERRIDE;
  virtual void ReleaseCapture(aura::Window* window) OVERRIDE;
  virtual aura::Window* GetCaptureWindow() OVERRIDE;
  virtual aura::Window* GetGlobalCaptureWindow() OVERRIDE;

 private:
  friend class ScopedCaptureClient;
  typedef std::set<aura::Window*> RootWindows;

  CaptureController();
  virtual ~CaptureController();

  // The current capture window. NULL if there is no capture window.
  aura::Window* capture_window_;

  // Set of RootWindows notified when capture changes.
  RootWindows root_windows_;

  DISALLOW_COPY_AND_ASSIGN(CaptureController);
};

// ScopedCaptureClient is responsible for creating a CaptureClient for a
// RootWindow. Specifically it creates a single CaptureController that is shared
// among all ScopedCaptureClients and adds the RootWindow to it.
class WM_EXPORT ScopedCaptureClient : public aura::WindowObserver {
 public:
  explicit ScopedCaptureClient(aura::Window* root);
  virtual ~ScopedCaptureClient();

  // Returns true if there is a CaptureController with at least one RootWindow.
  static bool IsActive();

  aura::client::CaptureClient* capture_client() {
    return capture_controller_;
  }

  // Overridden from aura::WindowObserver:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

 private:
  // Invoked from destructor and OnWindowDestroyed() to cleanup.
  void Shutdown();

  // The single CaptureController instance.
  static CaptureController* capture_controller_;

  // RootWindow this ScopedCaptureClient was create for.
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCaptureClient);
};

}  // namespace wm

#endif  // UI_WM_CORE_CAPTURE_CONTROLLER_H_
