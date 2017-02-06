// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_UI_BLIMP_WINDOW_TREE_CLIENT_H_
#define BLIMP_ENGINE_APP_UI_BLIMP_WINDOW_TREE_CLIENT_H_

#include "base/macros.h"
#include "ui/aura/client/window_tree_client.h"

namespace blimp {
namespace engine {

class BlimpWindowTreeClient : public aura::client::WindowTreeClient {
 public:
  // Caller ensures that |root_window| outlives this object.
  explicit BlimpWindowTreeClient(aura::Window* root_window);

  ~BlimpWindowTreeClient() override;

  // aura::client::WindowTreeClient interface.
  aura::Window* GetDefaultParent(aura::Window* context,
                                 aura::Window* window,
                                 const gfx::Rect& bounds) override;

 private:
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(BlimpWindowTreeClient);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_UI_BLIMP_WINDOW_TREE_CLIENT_H_
