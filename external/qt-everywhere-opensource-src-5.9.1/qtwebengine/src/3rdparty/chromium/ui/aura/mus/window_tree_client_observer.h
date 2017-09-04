// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_WINDOW_TREE_CLIENT_OBSERVER_H_
#define UI_AURA_MUS_WINDOW_TREE_CLIENT_OBSERVER_H_

#include "ui/aura/aura_export.h"

namespace aura {

class WindowTreeClient;

class AURA_EXPORT WindowTreeClientObserver {
 public:
  // Called early on in the destructor of WindowTreeClient; before any windows
  // have been destroyed.
  virtual void OnWillDestroyClient(WindowTreeClient* client) {}

  // Called at the end of WindowTreeClient's destructor. At this point
  // observers should drop all references to |client|.
  virtual void OnDidDestroyClient(WindowTreeClient* client) {}

 protected:
  virtual ~WindowTreeClientObserver() {}
};

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_TREE_CLIENT_OBSERVER_H_
