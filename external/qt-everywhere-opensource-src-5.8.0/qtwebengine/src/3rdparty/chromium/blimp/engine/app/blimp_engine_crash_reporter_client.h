// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_BLIMP_ENGINE_CRASH_REPORTER_CLIENT_H_
#define BLIMP_ENGINE_APP_BLIMP_ENGINE_CRASH_REPORTER_CLIENT_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "components/crash/content/app/crash_reporter_client.h"

namespace blimp {
namespace engine {

// Crash reporter client for the Blimp engine.  This will always collect crash
// reports, and will always upload reports unless running in a "headless" test
// mode.
// This is intended to be built with a Linux OS target only.
class BlimpEngineCrashReporterClient
    : public crash_reporter::CrashReporterClient {
 public:
  BlimpEngineCrashReporterClient();
  ~BlimpEngineCrashReporterClient() override;

 private:
  // CrashReporterClient implementation.
  void SetCrashReporterClientIdFromGUID(
      const std::string& client_guid) override;
  void GetProductNameAndVersion(const char** product_name,
                                const char** version) override;
  size_t RegisterCrashKeys() override;
  bool IsRunningUnattended() override;
  bool GetCollectStatsConsent() override;
  bool EnableBreakpadForProcess(const std::string& process_type) override;

  DISALLOW_COPY_AND_ASSIGN(BlimpEngineCrashReporterClient);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_BLIMP_ENGINE_CRASH_REPORTER_CLIENT_H_
