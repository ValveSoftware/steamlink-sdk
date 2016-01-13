// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/environment/environment.h"

#include <stddef.h>

#include "mojo/public/c/environment/logger.h"
#include "mojo/public/cpp/environment/lib/default_async_waiter.h"
#include "mojo/public/cpp/environment/lib/default_logger.h"
#include "mojo/public/cpp/utility/run_loop.h"

namespace mojo {

namespace {

const MojoAsyncWaiter* g_default_async_waiter = NULL;
const MojoLogger* g_default_logger = NULL;

void Init(const MojoAsyncWaiter* default_async_waiter,
          const MojoLogger* default_logger) {
  g_default_async_waiter =
      default_async_waiter ? default_async_waiter :
                             &internal::kDefaultAsyncWaiter;
  g_default_logger = default_logger ? default_logger :
                                      &internal::kDefaultLogger;

  RunLoop::SetUp();
}

}  // namespace

Environment::Environment() {
  Init(NULL, NULL);
}

Environment::Environment(const MojoAsyncWaiter* default_async_waiter,
                         const MojoLogger* default_logger) {
  Init(default_async_waiter, default_logger);
}

Environment::~Environment() {
  RunLoop::TearDown();

  // TODO(vtl): Maybe we should allow nesting, and restore previous default
  // async waiters and loggers?
}

// static
const MojoAsyncWaiter* Environment::GetDefaultAsyncWaiter() {
  return g_default_async_waiter;
}

// static
const MojoLogger* Environment::GetDefaultLogger() {
  return g_default_logger;
}

}  // namespace mojo
