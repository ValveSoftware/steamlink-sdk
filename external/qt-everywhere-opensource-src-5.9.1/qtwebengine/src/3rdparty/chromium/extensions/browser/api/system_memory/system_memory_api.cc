// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_memory/system_memory_api.h"

#include "extensions/browser/api/system_memory/memory_info_provider.h"

namespace extensions {

using api::system_memory::MemoryInfo;

SystemMemoryGetInfoFunction::SystemMemoryGetInfoFunction() {
}

SystemMemoryGetInfoFunction::~SystemMemoryGetInfoFunction() {
}

bool SystemMemoryGetInfoFunction::RunAsync() {
  MemoryInfoProvider::Get()->StartQueryInfo(
      base::Bind(&SystemMemoryGetInfoFunction::OnGetMemoryInfoCompleted, this));
  return true;
}

void SystemMemoryGetInfoFunction::OnGetMemoryInfoCompleted(bool success) {
  if (success)
    SetResult(MemoryInfoProvider::Get()->memory_info().ToValue());
  else
    SetError("Error occurred when querying memory information.");
  SendResponse(success);
}

}  // namespace extensions
