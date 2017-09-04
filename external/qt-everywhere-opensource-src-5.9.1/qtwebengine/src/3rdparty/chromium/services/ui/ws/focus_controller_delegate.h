// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_FOCUS_CONTROLLER_DELEGATE_H_
#define SERVICES_UI_WS_FOCUS_CONTROLLER_DELEGATE_H_

namespace ui {

namespace ws {

class ServerWindow;

class FocusControllerDelegate {
 public:
  virtual bool CanHaveActiveChildren(ServerWindow* window) const = 0;

 protected:
  ~FocusControllerDelegate() {}
};

}  // namespace ws

}  // namespace ui

#endif  // SERVICES_UI_WS_FOCUS_CONTROLLER_DELEGATE_H_
