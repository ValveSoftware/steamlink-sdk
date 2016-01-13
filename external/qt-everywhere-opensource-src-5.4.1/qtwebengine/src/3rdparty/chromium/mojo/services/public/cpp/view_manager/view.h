// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_H_

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "third_party/skia/include/core/SkColor.h"

class SkBitmap;

namespace mojo {
namespace view_manager {

class Node;
class ViewManager;
class ViewObserver;

// Views are owned by the ViewManager.
class View {
 public:
  static View* Create(ViewManager* manager);

  void Destroy();

  Id id() const { return id_; }
  Node* node() { return node_; }

  void AddObserver(ViewObserver* observer);
  void RemoveObserver(ViewObserver* observer);

  // TODO(beng): temporary only.
  void SetContents(const SkBitmap& contents);
  void SetColor(SkColor color);

 private:
  friend class ViewPrivate;

  explicit View(ViewManager* manager);
  View();
  ~View();

  void LocalDestroy();

  Id id_;
  Node* node_;
  ViewManager* manager_;

  ObserverList<ViewObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(View);
};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_H_
