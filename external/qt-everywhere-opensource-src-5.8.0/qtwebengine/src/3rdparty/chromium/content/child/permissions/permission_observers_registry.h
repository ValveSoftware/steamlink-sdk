// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_PERMISSIONS_PERMISSION_OBSERVERS_REGISTRY_H_
#define CONTENT_CHILD_PERMISSIONS_PERMISSION_OBSERVERS_REGISTRY_H_

#include <set>

namespace blink {
class WebPermissionObserver;
}

namespace content {

// PermissionObserversRegistry keeps a list of WebPermissionObserver with basic
// methods to add/removed/check. It is being used by PermissionDispatcher and
// PermissionDispatcherThreadProxy.
// This is not thread-safe.
class PermissionObserversRegistry {
 public:
  PermissionObserversRegistry();
  ~PermissionObserversRegistry();

  void RegisterObserver(blink::WebPermissionObserver* observer);
  void UnregisterObserver(blink::WebPermissionObserver* observer);
  bool IsObserverRegistered(blink::WebPermissionObserver* observer) const;

 private:
  std::set<blink::WebPermissionObserver*> observers_;
};

}  // namespace content

#endif // CONTENT_CHILD_PERMISSIONS_PERMISSION_OBSERVERS_REGISTRY_H_
