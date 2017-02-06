// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_CLIENT_OBSERVER_H_
#define COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_CLIENT_OBSERVER_H_

namespace mus {

class Window;
class WindowTreeClient;

class WindowTreeClientObserver {
 public:
  virtual void OnWindowTreeFocusChanged(Window* gained_focus,
                                        Window* lost_focus) {}

  // Called right before the client is destroyed.
  virtual void OnWillDestroyClient(WindowTreeClient* client) {}

 protected:
  virtual ~WindowTreeClientObserver() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_CLIENT_OBSERVER_H_
