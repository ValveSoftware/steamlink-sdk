// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/test_embedder.h"

#include <memory>

#include "base/logging.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/system/core.h"
#include "mojo/edk/system/handle_table.h"

namespace mojo {

namespace edk {
namespace internal {

bool ShutdownCheckNoLeaks(Core* core) {
  std::vector<MojoHandle> leaked_handles;
  core->GetActiveHandlesForTest(&leaked_handles);
  if (leaked_handles.empty())
    return true;
  for (auto handle : leaked_handles)
    LOG(ERROR) << "Mojo embedder shutdown: Leaking handle " << handle;
  return false;
}

}  // namespace internal

namespace test {

bool Shutdown() {
  CHECK(internal::g_core);
  bool rv = internal::ShutdownCheckNoLeaks(internal::g_core);
  delete internal::g_core;
  internal::g_core = nullptr;

  return rv;
}

}  // namespace test
}  // namespace edk

}  // namespace mojo
