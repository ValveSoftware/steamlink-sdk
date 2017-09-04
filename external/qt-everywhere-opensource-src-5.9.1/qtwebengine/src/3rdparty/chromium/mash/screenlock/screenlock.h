// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SCREENLOCK_SCREENLOCK_H_
#define MASH_SCREENLOCK_SCREENLOCK_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "mash/session/public/interfaces/session.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/tracing/public/cpp/provider.h"

namespace views {
class AuraInit;
class WindowManagerConnection;
}

namespace mash {
namespace screenlock {

class Screenlock : public service_manager::Service,
                   public session::mojom::ScreenlockStateListener {
 public:
  Screenlock();
  ~Screenlock() override;

 private:
  // service_manager::Service:
  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  // session::mojom::ScreenlockStateListener:
  void ScreenlockStateChanged(bool locked) override;

  tracing::Provider tracing_;
  std::unique_ptr<views::AuraInit> aura_init_;
  std::unique_ptr<views::WindowManagerConnection> window_manager_connection_;
  mojo::BindingSet<session::mojom::ScreenlockStateListener> bindings_;

  DISALLOW_COPY_AND_ASSIGN(Screenlock);
};

}  // namespace screenlock
}  // namespace mash

#endif  // MASH_SCREENLOCK_SCREENLOCK_H_
