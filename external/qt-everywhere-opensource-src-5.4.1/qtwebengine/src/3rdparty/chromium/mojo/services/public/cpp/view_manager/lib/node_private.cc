// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/lib/node_private.h"

namespace mojo {
namespace view_manager {

NodePrivate::NodePrivate(Node* node)
    : node_(node) {
}

NodePrivate::~NodePrivate() {
}

// static
Node* NodePrivate::LocalCreate() {
  return new Node;
}

}  // namespace view_manager
}  // namespace mojo
