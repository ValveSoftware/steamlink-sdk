// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_FOCUS_CONTROLLER_OBSERVER_H_
#define COMPONENTS_MUS_WS_FOCUS_CONTROLLER_OBSERVER_H_

namespace mus {
namespace ws {

enum class FocusControllerChangeSource;
class ServerWindow;

class FocusControllerObserver {
 public:
  virtual void OnActivationChanged(ServerWindow* old_active_window,
                                   ServerWindow* new_active_window) = 0;
  virtual void OnFocusChanged(FocusControllerChangeSource change_source,
                              ServerWindow* old_focused_window,
                              ServerWindow* new_focused_window) = 0;

 protected:
  ~FocusControllerObserver() {}
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_FOCUS_CONTROLLER_OBSERVER_H_
