// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_DELEGATE_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_DELEGATE_H_

namespace mojo {
namespace view_manager {

class Node;
class ViewManager;

class ViewManagerDelegate {
 public:
  virtual void OnRootAdded(ViewManager* view_manager, Node* root) {}

 protected:
  virtual ~ViewManagerDelegate() {}
};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_DELEGATE_H_
