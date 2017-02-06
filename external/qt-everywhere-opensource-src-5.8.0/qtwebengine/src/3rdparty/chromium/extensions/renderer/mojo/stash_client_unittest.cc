// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/macros.h"
#include "extensions/browser/mojo/stash_backend.h"
#include "extensions/common/mojo/stash.mojom.h"
#include "extensions/renderer/api_test_base.h"
#include "gin/dictionary.h"
#include "grit/extensions_renderer_resources.h"
#include "mojo/public/cpp/bindings/lib/message_builder.h"

// A test launcher for tests for the stash client defined in
// extensions/test/data/stash_client_unittest.js.

namespace extensions {
class StashClientTest : public ApiTestBase {
 public:
  StashClientTest() {}

  void SetUp() override {
    ApiTestBase::SetUp();
    stash_backend_.reset(new StashBackend(base::Closure()));
    PrepareEnvironment(api_test_env());
  }

  void PrepareEnvironment(ApiTestEnvironment* env) {
    env->service_provider()->AddService(base::Bind(
        &StashBackend::BindToRequest, base::Unretained(stash_backend_.get())));
  }

  std::unique_ptr<StashBackend> stash_backend_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StashClientTest);
};

// https://crbug.com/599898
#if defined(LEAK_SANITIZER)
#define MAYBE_StashAndRestore DISABLED_StashAndRestore
#else
#define MAYBE_StashAndRestore StashAndRestore
#endif
// Test that stashing and restoring work correctly.
TEST_F(StashClientTest, MAYBE_StashAndRestore) {
  ASSERT_NO_FATAL_FAILURE(RunTest("stash_client_unittest.js", "testStash"));
  std::unique_ptr<ModuleSystemTestEnvironment> restore_test_env(
      CreateEnvironment());
  ApiTestEnvironment restore_environment(restore_test_env.get());
  PrepareEnvironment(&restore_environment);
  restore_environment.RunTest("stash_client_unittest.js", "testRetrieve");
}

}  // namespace extensions
