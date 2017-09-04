// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_WINDOW_TREE_CLIENT_OBSERVER_H_
#define SERVICES_UI_PUBLIC_CPP_WINDOW_TREE_CLIENT_OBSERVER_H_

namespace ui {

class Window;
class WindowTreeClient;

class WindowTreeClientObserver {
 public:
  virtual void OnWindowTreeFocusChanged(Window* gained_focus,
                                        Window* lost_focus) {}

  virtual void OnWindowTreeCaptureChanged(Window* gained_capture,
                                          Window* lost_capture) {}

  // Called early on in the destructor of WindowTreeClient; before any windows
  // have been destroyed.
  virtual void OnWillDestroyClient(WindowTreeClient* client) {}

  // Called at the end of WindowTreeClient's destructor. At this point
  // observers should drop all references to |client|.
  virtual void OnDidDestroyClient(WindowTreeClient* client) {}

 protected:
  virtual ~WindowTreeClientObserver() {}
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_WINDOW_TREE_CLIENT_OBSERVER_H_
