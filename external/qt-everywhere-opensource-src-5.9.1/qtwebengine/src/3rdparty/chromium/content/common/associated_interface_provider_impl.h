// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/associated_interface_provider.h"

#include <stdint.h>
#include <memory>

#include "base/macros.h"
#include "content/common/associated_interfaces.mojom.h"

namespace content {

class AssociatedInterfaceProviderImpl : public AssociatedInterfaceProvider {
 public:
  // Binds this to a remote mojom::AssociatedInterfaceProvider.
  explicit AssociatedInterfaceProviderImpl(
      mojom::AssociatedInterfaceProviderAssociatedPtr proxy);
  // Constructs a local provider with no remote interfaces. This is useful in
  // conjunction with OverrideBinderForTesting(), in test environments where
  // there may not be a remote |mojom::AssociatedInterfaceProvider| available.
  AssociatedInterfaceProviderImpl();
  ~AssociatedInterfaceProviderImpl() override;

  // AssociatedInterfaceProvider:
  void GetInterface(const std::string& name,
                    mojo::ScopedInterfaceEndpointHandle handle) override;
  mojo::AssociatedGroup* GetAssociatedGroup() override;
  void OverrideBinderForTesting(
      const std::string& name,
      const base::Callback<void(mojo::ScopedInterfaceEndpointHandle)>& binder)
      override;

 private:
  class LocalProvider;

  mojom::AssociatedInterfaceProviderAssociatedPtr proxy_;

  std::unique_ptr<LocalProvider> local_provider_;

  DISALLOW_COPY_AND_ASSIGN(AssociatedInterfaceProviderImpl);
};

}  // namespace content
