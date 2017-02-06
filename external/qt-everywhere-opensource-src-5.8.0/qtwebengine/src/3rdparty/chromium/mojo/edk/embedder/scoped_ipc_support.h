// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_SCOPED_IPC_SUPPORT_H_
#define MOJO_EDK_EMBEDDER_SCOPED_IPC_SUPPORT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace edk {

// Performs any necessary Mojo initialization on construction, and shuts
// down Mojo on destruction.
//
// This should be instantiated once per process and retained as long as Mojo
// is needed. The TaskRunner passed to the constructor should outlive this
// object.
class MOJO_SYSTEM_IMPL_EXPORT ScopedIPCSupport {
 public:
  ScopedIPCSupport(scoped_refptr<base::TaskRunner> io_thread_task_runner);
  ~ScopedIPCSupport();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedIPCSupport);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_SCOPED_IPC_SUPPORT_H_
