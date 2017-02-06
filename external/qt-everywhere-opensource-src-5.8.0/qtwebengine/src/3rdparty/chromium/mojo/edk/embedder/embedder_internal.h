// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header contains internal details for the *implementation* of the
// embedder API. It should not be included by any public header (nor by users of
// the embedder API).

#ifndef MOJO_EDK_EMBEDDER_EMBEDDER_INTERNAL_H_
#define MOJO_EDK_EMBEDDER_EMBEDDER_INTERNAL_H_

#include <stdint.h>

#include "mojo/edk/system/system_impl_export.h"

namespace base {
class TaskRunner;
}

namespace mojo {

namespace edk {

class Broker;
class Core;
class ProcessDelegate;

namespace internal {

// Instance of |Broker| to use.
extern Broker* g_broker;

// Instance of |Core| used by the system functions (|Mojo...()|).
extern MOJO_SYSTEM_IMPL_EXPORT Core* g_core;
extern base::TaskRunner* g_delegate_thread_task_runner;
extern ProcessDelegate* g_process_delegate;

}  // namespace internal

}  // namepace edk

}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_EMBEDDER_INTERNAL_H_
