// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/special_storage_policy.h"

namespace storage {

SpecialStoragePolicy::Observer::~Observer() {}

SpecialStoragePolicy::SpecialStoragePolicy() {}

SpecialStoragePolicy::~SpecialStoragePolicy() {}

void SpecialStoragePolicy::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SpecialStoragePolicy::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SpecialStoragePolicy::NotifyGranted(const GURL& origin, int change_flags) {
  scoped_refptr<SpecialStoragePolicy> protect(this);
  FOR_EACH_OBSERVER(Observer, observers_, OnGranted(origin, change_flags));
}

void SpecialStoragePolicy::NotifyRevoked(const GURL& origin, int change_flags) {
  scoped_refptr<SpecialStoragePolicy> protect(this);
  FOR_EACH_OBSERVER(Observer, observers_, OnRevoked(origin, change_flags));
}

void SpecialStoragePolicy::NotifyCleared() {
  scoped_refptr<SpecialStoragePolicy> protect(this);
  FOR_EACH_OBSERVER(Observer, observers_, OnCleared());
}

}  // namespace storage
