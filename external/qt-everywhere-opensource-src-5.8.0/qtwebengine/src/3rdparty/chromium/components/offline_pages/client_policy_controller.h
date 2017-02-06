// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CLIENT_POLICY_CONTROLLER_H_
#define COMPONENTS_OFFLINE_PAGES_CLIENT_POLICY_CONTROLLER_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/time/time.h"
#include "components/offline_pages/offline_page_client_policy.h"

namespace offline_pages {

// This is the class which is a singleton for offline page model
// to get client policies based on namespaces.
class ClientPolicyController {
 public:
  ClientPolicyController();
  ~ClientPolicyController();

  // Generates a client policy from the input values.
  static const OfflinePageClientPolicy MakePolicy(
      const std::string& name_space,
      LifetimePolicy::LifetimeType lifetime_type,
      const base::TimeDelta& expiration_period,
      size_t page_limit,
      size_t pages_allowed_per_url);

  // Get the client policy for |name_space|.
  const OfflinePageClientPolicy& GetPolicy(const std::string& name_space) const;

 private:
  // The map from name_space to a client policy. Will be generated
  // as pre-defined values for now.
  std::map<std::string, OfflinePageClientPolicy> policies_;

  DISALLOW_COPY_AND_ASSIGN(ClientPolicyController);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CLIENT_POLICY_CONTROLLER_H_
