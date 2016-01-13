// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "sandbox/linux/suid/client/setuid_sandbox_client.h"
#include "sandbox/linux/suid/common/sandbox.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

TEST(SetuidSandboxClient, SetupLaunchEnvironment) {
  const char kTestValue[] = "This is a test";
  scoped_ptr<base::Environment> env(base::Environment::Create());
  EXPECT_TRUE(env != NULL);

  std::string saved_ld_preload;
  bool environment_had_ld_preload;
  // First, back-up the real LD_PRELOAD if any.
  environment_had_ld_preload = env->GetVar("LD_PRELOAD", &saved_ld_preload);
  // Setup environment variables to save or not save.
  EXPECT_TRUE(env->SetVar("LD_PRELOAD", kTestValue));
  EXPECT_TRUE(env->UnSetVar("LD_ORIGIN_PATH"));

  scoped_ptr<SetuidSandboxClient>
      sandbox_client(SetuidSandboxClient::Create());
  EXPECT_TRUE(sandbox_client != NULL);

  // Make sure the environment is clean.
  EXPECT_TRUE(env->UnSetVar(kSandboxEnvironmentApiRequest));
  EXPECT_TRUE(env->UnSetVar(kSandboxEnvironmentApiProvides));

  sandbox_client->SetupLaunchEnvironment();

  // Check if the requested API environment was set.
  std::string api_request;
  EXPECT_TRUE(env->GetVar(kSandboxEnvironmentApiRequest, &api_request));
  int api_request_num;
  EXPECT_TRUE(base::StringToInt(api_request, &api_request_num));
  EXPECT_EQ(api_request_num, kSUIDSandboxApiNumber);

  // Now check if LD_PRELOAD was saved to SANDBOX_LD_PRELOAD.
  std::string sandbox_ld_preload;
  EXPECT_TRUE(env->GetVar("SANDBOX_LD_PRELOAD", &sandbox_ld_preload));
  EXPECT_EQ(sandbox_ld_preload, kTestValue);

  // Check that LD_ORIGIN_PATH was not saved.
  EXPECT_FALSE(env->HasVar("SANDBOX_LD_ORIGIN_PATH"));

  // We should not forget to restore LD_PRELOAD at the end, or this environment
  // variable will affect the next running tests!
  if (environment_had_ld_preload) {
    EXPECT_TRUE(env->SetVar("LD_PRELOAD", saved_ld_preload));
  } else {
    EXPECT_TRUE(env->UnSetVar("LD_PRELOAD"));
  }
}

TEST(SetuidSandboxClient, SandboxedClientAPI) {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  EXPECT_TRUE(env != NULL);

  scoped_ptr<SetuidSandboxClient>
      sandbox_client(SetuidSandboxClient::Create());
  EXPECT_TRUE(sandbox_client != NULL);

  // Set-up a fake environment as if we went through the setuid sandbox.
  EXPECT_TRUE(env->SetVar(kSandboxEnvironmentApiProvides,
              base::IntToString(kSUIDSandboxApiNumber)));
  EXPECT_TRUE(env->SetVar(kSandboxDescriptorEnvironmentVarName, "1"));
  EXPECT_TRUE(env->SetVar(kSandboxPIDNSEnvironmentVarName, "1"));
  EXPECT_TRUE(env->UnSetVar(kSandboxNETNSEnvironmentVarName));

  // Check the API.
  EXPECT_TRUE(sandbox_client->IsSuidSandboxUpToDate());
  EXPECT_TRUE(sandbox_client->IsSuidSandboxChild());
  EXPECT_TRUE(sandbox_client->IsInNewPIDNamespace());
  EXPECT_FALSE(sandbox_client->IsInNewNETNamespace());

  // Forge an incorrect API version and check.
  EXPECT_TRUE(env->SetVar(kSandboxEnvironmentApiProvides,
              base::IntToString(kSUIDSandboxApiNumber + 1)));
  EXPECT_FALSE(sandbox_client->IsSuidSandboxUpToDate());
  // We didn't go through the actual sandboxing mechanism as it is
  // very hard in a unit test.
  EXPECT_FALSE(sandbox_client->IsSandboxed());
}

// This test doesn't accomplish much, but will make sure that analysis tools
// will run this codepath.
TEST(SetuidSandboxClient, GetSandboxBinaryPath) {
  scoped_ptr<SetuidSandboxClient> setuid_sandbox_client(
      SetuidSandboxClient::Create());
  ignore_result(setuid_sandbox_client->GetSandboxBinaryPath());
}

}  // namespace sandbox

