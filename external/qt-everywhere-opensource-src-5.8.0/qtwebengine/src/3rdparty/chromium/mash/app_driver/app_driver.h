// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_APP_DRIVER_APP_DRIVER_H_
#define MASH_APP_DRIVER_APP_DRIVER_H_

#include <stdint.h>

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/mus/public/interfaces/accelerator_registrar.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/shell/public/cpp/shell_client.h"

namespace mash {
namespace app_driver {

class AppDriver : public shell::ShellClient,
                  public mus::mojom::AcceleratorHandler {
 public:
  AppDriver();
  ~AppDriver() override;

 private:
  void OnAvailableCatalogEntries(mojo::Array<catalog::mojom::EntryPtr> entries);

  // shell::ShellClient:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(shell::Connection* connection) override;
  bool ShellConnectionLost() override;

  // mus::mojom::AcceleratorHandler:
  void OnAccelerator(uint32_t id, std::unique_ptr<ui::Event> event) override;

  void AddAccelerators();

  shell::Connector* connector_;
  catalog::mojom::CatalogPtr catalog_;
  mojo::Binding<mus::mojom::AcceleratorHandler> binding_;
  base::WeakPtrFactory<AppDriver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppDriver);
};

}  // namespace app_driver
}  // namespace mash

#endif  // MASH_APP_DRIVER_APP_DRIVER_H_
