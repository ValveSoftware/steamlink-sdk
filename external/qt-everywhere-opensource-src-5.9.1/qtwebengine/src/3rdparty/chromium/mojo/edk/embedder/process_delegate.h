// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_PROCESS_DELEGATE_H_
#define MOJO_EDK_EMBEDDER_PROCESS_DELEGATE_H_

#include "base/macros.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace edk {

// An interface for process delegates.
class MOJO_SYSTEM_IMPL_EXPORT ProcessDelegate {
 public:
  // Called when |ShutdownIPCSupport()| has completed work on the I/O thread.
  virtual void OnShutdownComplete() = 0;

 protected:
  ProcessDelegate() {}
  virtual ~ProcessDelegate() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessDelegate);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_PROCESS_DELEGATE_H_
