// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/syncable_prefs/pref_service_mock_factory.h"

#include "components/prefs/testing_pref_store.h"

namespace syncable_prefs {

PrefServiceMockFactory::PrefServiceMockFactory() {
    user_prefs_ = new TestingPrefStore;
}

PrefServiceMockFactory::~PrefServiceMockFactory() {}

}  // namespace syncable_prefs
