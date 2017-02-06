// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/permissions/permission_observers_registry.h"

#include "third_party/WebKit/public/platform/modules/permissions/WebPermissionObserver.h"

namespace content {

PermissionObserversRegistry::PermissionObserversRegistry() {
}

PermissionObserversRegistry::~PermissionObserversRegistry() {
}

void PermissionObserversRegistry::RegisterObserver(
    blink::WebPermissionObserver* observer) {
  observers_.insert(observer);
}

void PermissionObserversRegistry::UnregisterObserver(
    blink::WebPermissionObserver* observer) {
  observers_.erase(observer);
}

bool PermissionObserversRegistry::IsObserverRegistered(
    blink::WebPermissionObserver* observer) const {
  return observers_.find(observer) != observers_.end();
}

}  // namespace content
