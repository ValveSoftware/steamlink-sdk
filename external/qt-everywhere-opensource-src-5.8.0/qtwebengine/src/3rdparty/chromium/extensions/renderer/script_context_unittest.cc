// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/module_system_test.h"
#include "extensions/renderer/script_context.h"
#include "gin/per_context_data.h"
#include "gin/runner.h"

namespace extensions {

using ScriptContextTest = ModuleSystemTest;

TEST_F(ScriptContextTest, GinRunnerLifetime) {
  ExpectNoAssertionsMade();
  base::WeakPtr<gin::Runner> weak_runner =
      gin::PerContextData::From(env()->context()->v8_context())
          ->runner()
          ->GetWeakPtr();
  env()->ShutdownModuleSystem();
  EXPECT_FALSE(weak_runner);
}

}  // namespace extensions
