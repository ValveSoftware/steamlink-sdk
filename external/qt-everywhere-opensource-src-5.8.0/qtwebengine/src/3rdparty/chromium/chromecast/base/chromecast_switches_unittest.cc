// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chromecast/base/chromecast_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

TEST(ChromecastSwitchesTest, NoSwitch) {
  if (base::CommandLine::CommandLine::InitializedForCurrentProcess())
    base::CommandLine::Reset();
  base::CommandLine::Init(0, nullptr);
  EXPECT_TRUE(GetSwitchValueBoolean(switches::kEnableCmaMediaPipeline, true));
  EXPECT_FALSE(GetSwitchValueBoolean(switches::kEnableCmaMediaPipeline, false));
}

TEST(ChromecastSwitchesTest, NoSwitchValue) {
  if (base::CommandLine::CommandLine::InitializedForCurrentProcess())
    base::CommandLine::Reset();
  base::CommandLine::Init(0, nullptr);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(switches::kEnableCmaMediaPipeline);
  EXPECT_TRUE(GetSwitchValueBoolean(switches::kEnableCmaMediaPipeline, true));
  EXPECT_TRUE(GetSwitchValueBoolean(switches::kEnableCmaMediaPipeline, false));
}

TEST(ChromecastSwitchesTest, SwitchValueTrue) {
  if (base::CommandLine::CommandLine::InitializedForCurrentProcess())
    base::CommandLine::Reset();
  base::CommandLine::Init(0, nullptr);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitchASCII(switches::kEnableCmaMediaPipeline,
                        switches::kSwitchValueTrue);
  EXPECT_TRUE(GetSwitchValueBoolean(switches::kEnableCmaMediaPipeline, true));
  EXPECT_TRUE(GetSwitchValueBoolean(switches::kEnableCmaMediaPipeline, false));
}

TEST(ChromecastSwitchesTest, SwitchValueFalse) {
  if (base::CommandLine::CommandLine::InitializedForCurrentProcess())
    base::CommandLine::Reset();
  base::CommandLine::Init(0, nullptr);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitchASCII(switches::kEnableCmaMediaPipeline,
                        switches::kSwitchValueFalse);
  EXPECT_FALSE(GetSwitchValueBoolean(switches::kEnableCmaMediaPipeline, true));
  EXPECT_FALSE(GetSwitchValueBoolean(switches::kEnableCmaMediaPipeline, false));
}

TEST(ChromecastSwitchesTest, SwitchValueNonsense) {
  if (base::CommandLine::CommandLine::InitializedForCurrentProcess())
    base::CommandLine::Reset();
  base::CommandLine::Init(0, nullptr);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitchASCII(switches::kEnableCmaMediaPipeline, "silverware");
  EXPECT_TRUE(GetSwitchValueBoolean(switches::kEnableCmaMediaPipeline, true));
  EXPECT_FALSE(GetSwitchValueBoolean(switches::kEnableCmaMediaPipeline, false));
}

}  // namespace chromecast
