// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_MANAGER_WINDOW_TREE_FACTORY_SET_OBSERVER_H_
#define COMPONENTS_MUS_WS_WINDOW_MANAGER_WINDOW_TREE_FACTORY_SET_OBSERVER_H_

namespace mus {
namespace ws {

class WindowManagerWindowTreeFactory;

class WindowManagerWindowTreeFactorySetObserver {
 public:
  // Called when the WindowTree associated with |factory| has been set
  virtual void OnWindowManagerWindowTreeFactoryReady(
      WindowManagerWindowTreeFactory* factory) = 0;

 protected:
  virtual ~WindowManagerWindowTreeFactorySetObserver() {}
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_MANAGER_WINDOW_TREE_FACTORY_SET_OBSERVER_H_
