// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/mock_password_store.h"

#include "base/memory/ptr_util.h"

namespace password_manager {

MockPasswordStore::MockPasswordStore()
    : PasswordStore(base::ThreadTaskRunnerHandle::Get(),
                    base::ThreadTaskRunnerHandle::Get()) {
}

MockPasswordStore::~MockPasswordStore() {
}

std::vector<std::unique_ptr<InteractionsStats>>
MockPasswordStore::GetSiteStatsImpl(const GURL& origin_domain) {
  std::vector<InteractionsStats*> stats = GetSiteStatsMock(origin_domain);
  std::vector<std::unique_ptr<InteractionsStats>> result;
  result.reserve(stats.size());
  for (auto* stat : stats) {
    result.push_back(base::WrapUnique(stat));
  }
  return result;
}

}  // namespace password_manager
