// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNCABLE_PREFS_PREF_SERVICE_MOCK_FACTORY_H_
#define COMPONENTS_SYNCABLE_PREFS_PREF_SERVICE_MOCK_FACTORY_H_

#include "base/macros.h"
#include "components/syncable_prefs/pref_service_syncable_factory.h"

namespace syncable_prefs {

// A helper that allows convenient building of custom PrefServices in tests.
class PrefServiceMockFactory : public PrefServiceSyncableFactory {
 public:
  PrefServiceMockFactory();
  ~PrefServiceMockFactory() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefServiceMockFactory);
};

}  // namespace syncable_prefs

#endif  // COMPONENTS_SYNCABLE_PREFS_PREF_SERVICE_MOCK_FACTORY_H_
