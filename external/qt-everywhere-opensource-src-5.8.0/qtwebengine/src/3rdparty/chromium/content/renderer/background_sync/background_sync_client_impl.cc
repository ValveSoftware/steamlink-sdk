// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/background_sync/background_sync_client_impl.h"

#include <utility>

#include "content/child/background_sync/background_sync_provider.h"
#include "content/child/background_sync/background_sync_type_converters.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncProvider.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncRegistration.h"
#include "third_party/WebKit/public/web/modules/serviceworker/WebServiceWorkerContextProxy.h"

namespace content {

// static
void BackgroundSyncClientImpl::Create(
    mojo::InterfaceRequest<blink::mojom::BackgroundSyncServiceClient> request) {
  new BackgroundSyncClientImpl(std::move(request));
}

BackgroundSyncClientImpl::~BackgroundSyncClientImpl() {}

BackgroundSyncClientImpl::BackgroundSyncClientImpl(
    mojo::InterfaceRequest<blink::mojom::BackgroundSyncServiceClient> request)
    : binding_(this, std::move(request)) {}

void BackgroundSyncClientImpl::Sync(
    const mojo::String& tag,
    blink::mojom::BackgroundSyncEventLastChance last_chance,
    const SyncCallback& callback) {
  DCHECK(!blink::Platform::current()->mainThread()->isCurrentThread());

  ServiceWorkerContextClient* client =
      ServiceWorkerContextClient::ThreadSpecificInstance();
  if (!client) {
    callback.Run(blink::mojom::ServiceWorkerEventStatus::ABORTED);
    return;
  }

  blink::WebServiceWorkerContextProxy::LastChanceOption web_last_chance =
      mojo::ConvertTo<blink::WebServiceWorkerContextProxy::LastChanceOption>(
          last_chance);

  client->DispatchSyncEvent(tag, web_last_chance, callback);
}

}  // namespace content
