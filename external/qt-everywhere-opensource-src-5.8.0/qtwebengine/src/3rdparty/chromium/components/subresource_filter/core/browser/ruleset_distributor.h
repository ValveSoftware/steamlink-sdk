// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_RULESET_DISTRIBUTOR_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_RULESET_DISTRIBUTOR_H_

#include "base/files/file.h"

namespace subresource_filter {

// Defines the interface for the implementation responsible for distributing
// updated versions of subresource filtering rules to consumers. It is not
// intended as a general-purpose `subscriber` to the RulesetService. The extra
// level of indirection exists to allow the service to not depend on content/.
class RulesetDistributor {
 public:
  virtual ~RulesetDistributor() = default;

  // Instructs the distributor to redistribute the new version of the |ruleset|
  // to all existing consumers, and to distribute it to future consumers.
  virtual void PublishNewVersion(base::File ruleset_data) = 0;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_RULESET_DISTRIBUTOR_H_
