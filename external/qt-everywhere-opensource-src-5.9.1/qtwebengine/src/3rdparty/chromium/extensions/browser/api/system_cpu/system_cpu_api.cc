// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "extensions/browser/api/system_cpu/cpu_info_provider.h"
#include "extensions/browser/api/system_cpu/system_cpu_api.h"
#include "extensions/common/features/base_feature_provider.h"

namespace extensions {

using api::system_cpu::CpuInfo;

SystemCpuGetInfoFunction::SystemCpuGetInfoFunction() {
}

SystemCpuGetInfoFunction::~SystemCpuGetInfoFunction() {
}

bool SystemCpuGetInfoFunction::RunAsync() {
  CpuInfoProvider::Get()->StartQueryInfo(
      base::Bind(&SystemCpuGetInfoFunction::OnGetCpuInfoCompleted, this));
  return true;
}

void SystemCpuGetInfoFunction::OnGetCpuInfoCompleted(bool success) {
  if (success)
    SetResult(CpuInfoProvider::Get()->cpu_info().ToValue());
  else
    SetError("Error occurred when querying cpu information.");
  SendResponse(success);
}

}  // namespace extensions
