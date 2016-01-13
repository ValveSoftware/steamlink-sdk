// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_ROOT_VIEW_MANAGER_DELEGATE_H_
#define MOJO_SERVICES_VIEW_MANAGER_ROOT_VIEW_MANAGER_DELEGATE_H_

#include "mojo/services/view_manager/view_manager_export.h"

namespace mojo {
namespace view_manager {
namespace service {

class MOJO_VIEW_MANAGER_EXPORT RootViewManagerDelegate {
 public:
  // Invoked when the WindowTreeHost is ready.
  virtual void OnRootViewManagerWindowTreeHostCreated() = 0;

 protected:
  virtual ~RootViewManagerDelegate() {}
};

}  // namespace service
}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_ROOT_VIEW_MANAGER_H_
