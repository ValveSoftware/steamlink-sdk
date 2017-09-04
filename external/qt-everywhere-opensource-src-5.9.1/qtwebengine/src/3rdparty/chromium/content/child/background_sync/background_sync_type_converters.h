// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_TYPE_CONVERTERS_H_
#define CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_TYPE_CONVERTERS_H_

#include <memory>

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncError.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncRegistration.h"
#include "third_party/WebKit/public/platform/modules/background_sync/background_sync.mojom.h"
#include "third_party/WebKit/public/web/modules/serviceworker/WebServiceWorkerContextProxy.h"

namespace mojo {

// blink::WebSyncRegistration::NetworkState <=>
//     blink::mojom::BackgroundSyncNetworkState

template <>
struct CONTENT_EXPORT TypeConverter<blink::WebSyncRegistration::NetworkState,
                                    blink::mojom::BackgroundSyncNetworkState> {
  static blink::WebSyncRegistration::NetworkState Convert(
      blink::mojom::BackgroundSyncNetworkState input);
};

template <>
struct CONTENT_EXPORT TypeConverter<blink::mojom::BackgroundSyncNetworkState,
                                    blink::WebSyncRegistration::NetworkState> {
  static blink::mojom::BackgroundSyncNetworkState Convert(
      blink::WebSyncRegistration::NetworkState input);
};

// blink::WebSyncRegistration <=>
//     blink::mojom::SyncRegistration

template <>
struct CONTENT_EXPORT TypeConverter<std::unique_ptr<blink::WebSyncRegistration>,
                                    blink::mojom::SyncRegistrationPtr> {
  static std::unique_ptr<blink::WebSyncRegistration> Convert(
      const blink::mojom::SyncRegistrationPtr& input);
};

template <>
struct CONTENT_EXPORT TypeConverter<blink::mojom::SyncRegistrationPtr,
                                    blink::WebSyncRegistration> {
  static blink::mojom::SyncRegistrationPtr Convert(
      const blink::WebSyncRegistration& input);
};

// blink::WebServiceWorkerContextProxy::LastChanceOption <=>
//    blink::mojom::BackgroundSyncEventLastChance

template <>
struct CONTENT_EXPORT
    TypeConverter<blink::WebServiceWorkerContextProxy::LastChanceOption,
                  blink::mojom::BackgroundSyncEventLastChance> {
  static blink::WebServiceWorkerContextProxy::LastChanceOption Convert(
      blink::mojom::BackgroundSyncEventLastChance input);
};

template <>
struct CONTENT_EXPORT
    TypeConverter<blink::mojom::BackgroundSyncEventLastChance,
                  blink::WebServiceWorkerContextProxy::LastChanceOption> {
  static blink::mojom::BackgroundSyncEventLastChance Convert(
      blink::WebServiceWorkerContextProxy::LastChanceOption input);
};

}  // namespace mojo

#endif  // CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_TYPE_CONVERTERS_H_
