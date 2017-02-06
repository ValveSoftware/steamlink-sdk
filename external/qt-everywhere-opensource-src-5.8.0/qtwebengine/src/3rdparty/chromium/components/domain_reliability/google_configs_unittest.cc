// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/google_configs.h"

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {

namespace {

typedef std::vector<DomainReliabilityConfig*> ConfigPointerVector;

TEST(DomainReliabilityGoogleConfigsTest, Enumerate) {
  ConfigPointerVector configs;
  STLElementDeleter<ConfigPointerVector> configs_deleter(&configs);

  GetAllGoogleConfigs(&configs);
}

TEST(DomainReliabilityGoogleConfigsTest, ConfigsAreValid) {
  ConfigPointerVector configs;
  STLElementDeleter<ConfigPointerVector> configs_deleter(&configs);

  GetAllGoogleConfigs(&configs);
  for (auto config : configs)
    EXPECT_TRUE(config->IsValid());
}

}  // namespace

}  // namespace domain_reliability
