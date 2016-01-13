// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_AURA_DEMO_WINDOW_TREE_HOST_VIEW_MANAGER_DELEGATE_H_
#define MOJO_EXAMPLES_AURA_DEMO_WINDOW_TREE_HOST_VIEW_MANAGER_DELEGATE_H_

class SkBitmap;

namespace mojo {

class WindowTreeHostMojoDelegate {
 public:
  // Invoked when the contents of the composite associated with the
  // WindowTreeHostMojo are updated.
  virtual void CompositorContentsChanged(const SkBitmap& bitmap) = 0;

 protected:
  virtual ~WindowTreeHostMojoDelegate() {}
};

}  // namespace mojo

#endif  // MOJO_EXAMPLES_AURA_DEMO_WINDOW_TREE_HOST_VIEW_MANAGER_DELEGATE_H_
