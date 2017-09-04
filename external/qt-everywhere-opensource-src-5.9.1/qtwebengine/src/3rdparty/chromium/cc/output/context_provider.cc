// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/context_provider.h"

namespace cc {

ContextProvider::ScopedContextLock::ScopedContextLock(
    ContextProvider* context_provider)
    : context_provider_(context_provider),
      context_lock_(*context_provider_->GetLock()) {
  // Allow current thread to use |context_provider_|.
  context_provider_->DetachFromThread();
  busy_ = context_provider_->CacheController()->ClientBecameBusy();
}

ContextProvider::ScopedContextLock::~ScopedContextLock() {
  // Let ContextCacheController know we are no longer busy.
  context_provider_->CacheController()->ClientBecameNotBusy(std::move(busy_));

  // Allow usage by thread for which |context_provider_| is bound to.
  context_provider_->DetachFromThread();
}

}  // namespace cc
