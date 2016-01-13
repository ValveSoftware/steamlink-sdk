// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_H_

#include <vector>

#include "base/logging.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/view_manager_export.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace mojo {
namespace view_manager {
namespace service {
class Node;

// Represents a view. A view may be associated with a single Node.
class MOJO_VIEW_MANAGER_EXPORT View {
 public:
  explicit View(const ViewId& id);
  ~View();

  const ViewId& id() const { return id_; }

  Node* node() { return node_; }

  void SetBitmap(const SkBitmap& contents);
  const SkBitmap& bitmap() const { return bitmap_; }

 private:
  // Node is responsible for maintaining |node_|.
  friend class Node;

  void set_node(Node* node) { node_ = node; }

  const ViewId id_;
  Node* node_;
  SkBitmap bitmap_;

  DISALLOW_COPY_AND_ASSIGN(View);
};

}  // namespace service
}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_H_
