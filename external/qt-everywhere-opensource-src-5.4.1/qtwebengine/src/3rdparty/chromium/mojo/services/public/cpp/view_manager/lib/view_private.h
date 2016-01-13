// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_PRIVATE_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_PRIVATE_H_

#include "base/basictypes.h"

#include "mojo/services/public/cpp/view_manager/view.h"

namespace mojo {
namespace view_manager {

class ViewPrivate {
 public:
  explicit ViewPrivate(View* view);
  ~ViewPrivate();

  static View* LocalCreate();
  void LocalDestroy() {
    view_->LocalDestroy();
  }

  void set_id(Id id) { view_->id_ = id; }
  void set_node(Node* node) { view_->node_ = node; }

  ViewManager* view_manager() { return view_->manager_; }
  void set_view_manager(ViewManager* manager) {
    view_->manager_ = manager;
  }

  ObserverList<ViewObserver>* observers() { return &view_->observers_; }

 private:
  View* view_;

  DISALLOW_COPY_AND_ASSIGN(ViewPrivate);
};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_PRIVATE_H_
