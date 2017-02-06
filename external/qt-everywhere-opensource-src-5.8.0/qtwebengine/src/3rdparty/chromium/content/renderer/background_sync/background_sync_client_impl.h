// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BACKGROUND_SYNC_BACKGROUND_SYNC_CLIENT_IMPL_H_
#define CONTENT_RENDERER_BACKGROUND_SYNC_BACKGROUND_SYNC_CLIENT_IMPL_H_

#include <stdint.h>

#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/WebKit/public/platform/modules/background_sync/background_sync.mojom.h"

namespace content {

class CONTENT_EXPORT BackgroundSyncClientImpl
    : public NON_EXPORTED_BASE(blink::mojom::BackgroundSyncServiceClient) {
 public:
  static void Create(mojo::InterfaceRequest<
                     blink::mojom::BackgroundSyncServiceClient> request);

  ~BackgroundSyncClientImpl() override;

 private:
  explicit BackgroundSyncClientImpl(
      mojo::InterfaceRequest<blink::mojom::BackgroundSyncServiceClient>
          request);

  // BackgroundSyncServiceClient methods:
  void Sync(const mojo::String& tag,
            blink::mojom::BackgroundSyncEventLastChance last_chance,
            const SyncCallback& callback) override;

  mojo::StrongBinding<blink::mojom::BackgroundSyncServiceClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncClientImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_BACKGROUND_SYNC_BACKGROUND_SYNC_CLIENT_IMPL_H_
