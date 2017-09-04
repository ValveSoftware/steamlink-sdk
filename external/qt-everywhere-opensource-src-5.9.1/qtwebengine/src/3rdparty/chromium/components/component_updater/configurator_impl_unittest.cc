// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "components/component_updater/configurator_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {

const int kDelayOneMinute = 60;
const int kDelayOneHour = kDelayOneMinute * 60;

}  // namespace

using base::CommandLine;

class ComponentUpdaterConfiguratorImplTest : public testing::Test {
 public:
  ComponentUpdaterConfiguratorImplTest() {}
  ~ComponentUpdaterConfiguratorImplTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ComponentUpdaterConfiguratorImplTest);
};

TEST_F(ComponentUpdaterConfiguratorImplTest, FastUpdate) {
  // Test the default timing values when no command line argument is present.
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);
  std::unique_ptr<ConfiguratorImpl> config(
      new ConfiguratorImpl(&cmdline, nullptr, false));
  CHECK_EQ(6 * kDelayOneMinute, config->InitialDelay());
  CHECK_EQ(6 * kDelayOneHour, config->NextCheckDelay());
  CHECK_EQ(1, config->StepDelay());
  CHECK_EQ(30 * kDelayOneMinute, config->OnDemandDelay());
  CHECK_EQ(15 * kDelayOneMinute, config->UpdateDelay());

  // Test the fast-update timings.
  cmdline.AppendSwitchASCII("--component-updater", "fast-update");
  config.reset(new ConfiguratorImpl(&cmdline, nullptr, false));
  CHECK_EQ(10, config->InitialDelay());
  CHECK_EQ(6 * kDelayOneHour, config->NextCheckDelay());
  CHECK_EQ(1, config->StepDelay());
  CHECK_EQ(2, config->OnDemandDelay());
  CHECK_EQ(10, config->UpdateDelay());
}

}  // namespace component_updater
