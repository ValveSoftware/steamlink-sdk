// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_util.h"

#include <memory>

#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "gpu/config/gpu_control_list_jsons.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_switches.h"

namespace gpu {

TEST(GpuUtilTest, MergeFeatureSets) {
  {
    // Merge two empty sets.
    std::set<int> src;
    std::set<int> dst;
    EXPECT_TRUE(dst.empty());
    MergeFeatureSets(&dst, src);
    EXPECT_TRUE(dst.empty());
  }
  {
    // Merge an empty set into a set with elements.
    std::set<int> src;
    std::set<int> dst;
    dst.insert(1);
    EXPECT_EQ(1u, dst.size());
    MergeFeatureSets(&dst, src);
    EXPECT_EQ(1u, dst.size());
  }
  {
    // Merge two sets where the source elements are already in the target set.
    std::set<int> src;
    std::set<int> dst;
    src.insert(1);
    dst.insert(1);
    EXPECT_EQ(1u, dst.size());
    MergeFeatureSets(&dst, src);
    EXPECT_EQ(1u, dst.size());
  }
  {
    // Merge two sets with different elements.
    std::set<int> src;
    std::set<int> dst;
    src.insert(1);
    dst.insert(2);
    EXPECT_EQ(1u, dst.size());
    MergeFeatureSets(&dst, src);
    EXPECT_EQ(2u, dst.size());
  }
}

TEST(GpuUtilTest, StringToFeatureSet) {
  {
    // zero feature.
    std::set<int> features;
    StringToFeatureSet("", &features);
    EXPECT_EQ(0u, features.size());
  }
  {
    // One features.
    std::set<int> features;
    StringToFeatureSet("4", &features);
    EXPECT_EQ(1u, features.size());
  }
  {
    // Multiple features.
    std::set<int> features;
    StringToFeatureSet("1,9", &features);
    EXPECT_EQ(2u, features.size());
  }
}

TEST(GpuUtilTest,
     ApplyGpuDriverBugWorkarounds_DisabledExtensions) {
  GPUInfo gpu_info;
  CollectBasicGraphicsInfo(&gpu_info);
  std::unique_ptr<GpuDriverBugList> list(GpuDriverBugList::Create());
  list->LoadList(kGpuDriverBugListJson, GpuControlList::kCurrentOsOnly);
  list->MakeDecision(GpuControlList::kOsAny, std::string(), gpu_info);
  std::vector<std::string> expected_disabled_extensions =
      list->GetDisabledExtensions();
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  ApplyGpuDriverBugWorkarounds(gpu_info, &command_line);

  std::vector<std::string> actual_disabled_extensions = base::SplitString(
      command_line.GetSwitchValueASCII(switches::kDisableGLExtensions), ", ;",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  sort(expected_disabled_extensions.begin(),
       expected_disabled_extensions.end());
  sort(actual_disabled_extensions.begin(), actual_disabled_extensions.end());

  EXPECT_EQ(expected_disabled_extensions, actual_disabled_extensions);
}

TEST(GpuUtilTest, ParseSecondaryGpuDevicesFromCommandLine_Simple) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kGpuSecondaryVendorIDs, "0x10de");
  command_line.AppendSwitchASCII(switches::kGpuSecondaryDeviceIDs, "0x0de1");

  GPUInfo gpu_info;
  ParseSecondaryGpuDevicesFromCommandLine(command_line, &gpu_info);

  EXPECT_EQ(gpu_info.secondary_gpus.size(), 1ul);
  EXPECT_EQ(gpu_info.secondary_gpus[0].vendor_id, 0x10deul);
  EXPECT_EQ(gpu_info.secondary_gpus[0].device_id, 0x0de1ul);
}

TEST(GpuUtilTest, ParseSecondaryGpuDevicesFromCommandLine_Multiple) {
  std::vector<std::pair<uint32_t, uint32_t>> gpu_devices = {
      {0x10de, 0x0de1}, {0x1002, 0x6779}, {0x8086, 0x040a}};

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kGpuSecondaryVendorIDs,
                                 "0x10de;0x1002;0x8086");
  command_line.AppendSwitchASCII(switches::kGpuSecondaryDeviceIDs,
                                 "0x0de1;0x6779;0x040a");

  GPUInfo gpu_info;
  ParseSecondaryGpuDevicesFromCommandLine(command_line, &gpu_info);
  EXPECT_EQ(gpu_info.secondary_gpus.size(), 3ul);
  EXPECT_EQ(gpu_info.secondary_gpus.size(), gpu_devices.size());

  for (size_t i = 0; i < gpu_info.secondary_gpus.size(); ++i) {
    EXPECT_EQ(gpu_info.secondary_gpus[i].vendor_id, gpu_devices[i].first);
    EXPECT_EQ(gpu_info.secondary_gpus[i].device_id, gpu_devices[i].second);
  }
}

TEST(GpuUtilTest, ParseSecondaryGpuDevicesFromCommandLine_Generated) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  std::vector<std::pair<uint32_t, uint32_t>> gpu_devices;

  std::string vendor_ids_str;
  std::string device_ids_str;
  for (uint32_t i = 0x80000000; i > 1; i >>= 1) {
    gpu_devices.push_back(std::pair<uint32_t, uint32_t>(i, i + 1));

    if (!vendor_ids_str.empty())
      vendor_ids_str += ";";
    if (!device_ids_str.empty())
      device_ids_str += ";";
    vendor_ids_str += base::StringPrintf("0x%04x", i);
    device_ids_str += base::StringPrintf("0x%04x", i + 1);
  }

  command_line.AppendSwitchASCII(switches::kGpuSecondaryVendorIDs,
                                 vendor_ids_str);
  command_line.AppendSwitchASCII(switches::kGpuSecondaryDeviceIDs,
                                 device_ids_str);

  GPUInfo gpu_info;
  ParseSecondaryGpuDevicesFromCommandLine(command_line, &gpu_info);

  EXPECT_EQ(gpu_devices.size(), 31ul);
  EXPECT_EQ(gpu_info.secondary_gpus.size(), gpu_devices.size());

  for (size_t i = 0; i < gpu_info.secondary_gpus.size(); ++i) {
    EXPECT_EQ(gpu_info.secondary_gpus[i].vendor_id, gpu_devices[i].first);
    EXPECT_EQ(gpu_info.secondary_gpus[i].device_id, gpu_devices[i].second);
  }
}

TEST(GpuUtilTest, ParseSecondaryGpuDevicesFromCommandLine_Testing) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);

  command_line.AppendSwitchASCII(switches::kGpuSecondaryVendorIDs, "0x10de");
  command_line.AppendSwitchASCII(switches::kGpuSecondaryDeviceIDs, "0x0de1");

  command_line.AppendSwitchASCII(switches::kGpuTestingSecondaryVendorIDs,
                                 "0x10de;0x1002");
  command_line.AppendSwitchASCII(switches::kGpuTestingSecondaryDeviceIDs,
                                 "0x0de1;0x6779");

  GPUInfo gpu_info;
  ParseSecondaryGpuDevicesFromCommandLine(command_line, &gpu_info);

  EXPECT_EQ(gpu_info.secondary_gpus.size(), 2ul);
  EXPECT_EQ(gpu_info.secondary_gpus[0].vendor_id, 0x10deul);
  EXPECT_EQ(gpu_info.secondary_gpus[0].device_id, 0x0de1ul);
  EXPECT_EQ(gpu_info.secondary_gpus[1].vendor_id, 0x1002ul);
  EXPECT_EQ(gpu_info.secondary_gpus[1].device_id, 0x6779ul);
}

TEST(GpuUtilTest, ParseSecondaryGpuDevicesFromCommandLine_TestingClear) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);

  command_line.AppendSwitchASCII(switches::kGpuSecondaryVendorIDs,
                                 "0x1002;0x10de");
  command_line.AppendSwitchASCII(switches::kGpuSecondaryDeviceIDs,
                                 "0x6779;0x0de1");

  GPUInfo gpu_info;
  ParseSecondaryGpuDevicesFromCommandLine(command_line, &gpu_info);
  EXPECT_EQ(gpu_info.secondary_gpus.size(), 2ul);

  command_line.AppendSwitchASCII(switches::kGpuTestingSecondaryVendorIDs, "");
  command_line.AppendSwitchASCII(switches::kGpuTestingSecondaryDeviceIDs, "");

  ParseSecondaryGpuDevicesFromCommandLine(command_line, &gpu_info);
  EXPECT_EQ(gpu_info.secondary_gpus.size(), 0ul);
}

}  // namespace gpu
