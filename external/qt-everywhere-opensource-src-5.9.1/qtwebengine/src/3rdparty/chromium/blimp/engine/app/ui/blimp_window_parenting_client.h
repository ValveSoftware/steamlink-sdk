// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_UI_BLIMP_WINDOW_PARENTING_CLIENT_H_
#define BLIMP_ENGINE_APP_UI_BLIMP_WINDOW_PARENTING_CLIENT_H_

#include "base/macros.h"
#include "ui/aura/client/window_parenting_client.h"

namespace blimp {
namespace engine {

class BlimpWindowParentingClient : public aura::client::WindowParentingClient {
 public:
  // Caller ensures that |root_window| outlives this object.
  explicit BlimpWindowParentingClient(aura::Window* root_window);

  ~BlimpWindowParentingClient() override;

  // aura::client::WindowParentingClient interface.
  aura::Window* GetDefaultParent(aura::Window* context,
                                 aura::Window* window,
                                 const gfx::Rect& bounds) override;

 private:
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(BlimpWindowParentingClient);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_UI_BLIMP_WINDOW_PARENTING_CLIENT_H_
