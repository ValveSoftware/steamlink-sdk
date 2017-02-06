// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_util.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "gpu/config/gpu_control_list_jsons.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "ui/gl/gl_switches.h"

namespace gpu {

namespace {

// Combine the integers into a string, seperated by ','.
std::string IntSetToString(const std::set<int>& list) {
  std::string rt;
  for (std::set<int>::const_iterator it = list.begin();
       it != list.end(); ++it) {
    if (!rt.empty())
      rt += ",";
    rt += base::IntToString(*it);
  }
  return rt;
}

void StringToIntSet(const std::string& str, std::set<int>* list) {
  DCHECK(list);
  for (const base::StringPiece& piece :
       base::SplitStringPiece(str, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_ALL)) {
    int number = 0;
    bool succeed = base::StringToInt(piece, &number);
    DCHECK(succeed);
    list->insert(number);
  }
}

// |str| is in the format of "0x040a;0x10de;...;hex32_N".
void StringToIds(const std::string& str, std::vector<uint32_t>* list) {
  DCHECK(list);
  for (const base::StringPiece& piece : base::SplitStringPiece(
           str, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    uint32_t id = 0;
    bool succeed = base::HexStringToUInt(piece, &id);
    DCHECK(succeed);
    list->push_back(id);
  }
}

}  // namespace anonymous

void MergeFeatureSets(std::set<int>* dst, const std::set<int>& src) {
  DCHECK(dst);
  if (src.empty())
    return;
  dst->insert(src.begin(), src.end());
}

void ApplyGpuDriverBugWorkarounds(const GPUInfo& gpu_info,
                                  base::CommandLine* command_line) {
  std::unique_ptr<GpuDriverBugList> list(GpuDriverBugList::Create());
  list->LoadList(kGpuDriverBugListJson,
                 GpuControlList::kCurrentOsOnly);
  std::set<int> workarounds = list->MakeDecision(
      GpuControlList::kOsAny, std::string(), gpu_info);
  GpuDriverBugList::AppendWorkaroundsFromCommandLine(
      &workarounds, *command_line);
  if (!workarounds.empty()) {
    command_line->AppendSwitchASCII(switches::kGpuDriverBugWorkarounds,
                                    IntSetToString(workarounds));
  }

  std::set<std::string> disabled_extensions;
  std::vector<std::string> buglist_disabled_extensions =
      list->GetDisabledExtensions();
  disabled_extensions.insert(buglist_disabled_extensions.begin(),
                             buglist_disabled_extensions.end());

  if (command_line->HasSwitch(switches::kDisableGLExtensions)) {
    std::vector<std::string> existing_disabled_extensions = base::SplitString(
        command_line->GetSwitchValueASCII(switches::kDisableGLExtensions), " ",
        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    disabled_extensions.insert(existing_disabled_extensions.begin(),
                               existing_disabled_extensions.end());
  }

  if (!disabled_extensions.empty()) {
    std::vector<std::string> v(disabled_extensions.begin(),
                               disabled_extensions.end());
    command_line->AppendSwitchASCII(switches::kDisableGLExtensions,
                                    base::JoinString(v, " "));
  }
}

void StringToFeatureSet(
    const std::string& str, std::set<int>* feature_set) {
  StringToIntSet(str, feature_set);
}

void ParseSecondaryGpuDevicesFromCommandLine(
    const base::CommandLine& command_line,
    GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  const char* secondary_vendor_switch_key = switches::kGpuSecondaryVendorIDs;
  const char* secondary_device_switch_key = switches::kGpuSecondaryDeviceIDs;

  if (command_line.HasSwitch(switches::kGpuTestingSecondaryVendorIDs) &&
      command_line.HasSwitch(switches::kGpuTestingSecondaryDeviceIDs)) {
    secondary_vendor_switch_key = switches::kGpuTestingSecondaryVendorIDs;
    secondary_device_switch_key = switches::kGpuTestingSecondaryDeviceIDs;
  }

  if (!command_line.HasSwitch(secondary_vendor_switch_key) ||
      !command_line.HasSwitch(secondary_device_switch_key)) {
    return;
  }

  std::vector<uint32_t> vendor_ids;
  std::vector<uint32_t> device_ids;
  StringToIds(command_line.GetSwitchValueASCII(secondary_vendor_switch_key),
              &vendor_ids);
  StringToIds(command_line.GetSwitchValueASCII(secondary_device_switch_key),
              &device_ids);

  DCHECK(vendor_ids.size() == device_ids.size());
  gpu_info->secondary_gpus.clear();
  for (size_t i = 0; i < vendor_ids.size() && i < device_ids.size(); ++i) {
    gpu::GPUInfo::GPUDevice secondary_device;
    secondary_device.active = false;
    secondary_device.vendor_id = vendor_ids[i];
    secondary_device.device_id = device_ids[i];
    gpu_info->secondary_gpus.push_back(secondary_device);
  }
}

}  // namespace gpu
