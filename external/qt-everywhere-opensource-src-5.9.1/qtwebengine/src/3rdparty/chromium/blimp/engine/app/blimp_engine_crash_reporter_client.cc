// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_engine_crash_reporter_client.h"

#include "blimp/engine/app/blimp_engine_crash_keys.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/version_info/version_info_values.h"
#include "content/public/common/content_switches.h"

namespace blimp {
namespace engine {

namespace {
// The product name that will be reported to breakpad.
const char kProductName[] = "Chrome_Blimp_Engine";
}  // namespace

BlimpEngineCrashReporterClient::BlimpEngineCrashReporterClient() {}
BlimpEngineCrashReporterClient::~BlimpEngineCrashReporterClient() {}

void BlimpEngineCrashReporterClient::SetCrashReporterClientIdFromGUID(
    const std::string& client_guid) {
  ::crash_keys::SetMetricsClientIdFromGUID(client_guid);
}

void BlimpEngineCrashReporterClient::GetProductNameAndVersion(
    const char** product_name,
    const char** version) {
  *product_name = kProductName;
  *version = PRODUCT_VERSION;
}

size_t BlimpEngineCrashReporterClient::RegisterCrashKeys() {
  return RegisterEngineCrashKeys();
}

bool BlimpEngineCrashReporterClient::IsRunningUnattended() {
  // If this returns "true," crash reports will not be uploaded. For now the
  // engine will not be running unattended. Eventually when automated testing
  // harnesses are set up, this should be changed to return "true" in those
  // cases.
  return false;
}

bool BlimpEngineCrashReporterClient::GetCollectStatsConsent() {
  // Always collect Blimp engine crash reports on official builds.
#ifdef OFFICIAL_BUILD
  return true;
#else
  return false;
#endif  // OFFICIAL_BUILD
}

bool BlimpEngineCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return process_type == ::switches::kRendererProcess ||
         process_type == ::switches::kPpapiPluginProcess ||
         process_type == ::switches::kZygoteProcess ||
         process_type == ::switches::kGpuProcess;
}

}  // namespace engine
}  // namespace blimp
