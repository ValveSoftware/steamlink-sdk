// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_coordinator/child/child_memory_coordinator_impl.h"

namespace memory_coordinator {

ChildMemoryCoordinatorImpl::ChildMemoryCoordinatorImpl(
      mojo::InterfaceRequest<mojom::ChildMemoryCoordinator> request,
      scoped_refptr<ClientList> clients)
    : binding_(this, std::move(request)),
      clients_(clients) {}

ChildMemoryCoordinatorImpl::~ChildMemoryCoordinatorImpl() {}

void ChildMemoryCoordinatorImpl::OnStateChange(mojom::MemoryState state) {
  clients_->Notify(FROM_HERE, &MemoryCoordinatorClient::OnMemoryStateChange,
                   state);
}

}  // namespace memory_coordinator
