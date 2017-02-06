// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_UTILITY_PROCESS_CONTROL_IMPL_H_
#define CONTENT_UTILITY_UTILITY_PROCESS_CONTROL_IMPL_H_

#include "base/macros.h"
#include "content/child/process_control_impl.h"

namespace content {

// Customization of ProcessControlImpl for the utility process. Exposed to the
// browser via the utility process's InterfaceRegistry.
class UtilityProcessControlImpl : public ProcessControlImpl {
 public:
  UtilityProcessControlImpl();
  ~UtilityProcessControlImpl() override;

  // ProcessControlImpl:
  void RegisterApplications(ApplicationMap* apps) override;
  void OnApplicationQuit() override;

 private:
  void OnLoadFailed() override;

  DISALLOW_COPY_AND_ASSIGN(UtilityProcessControlImpl);
};

}  // namespace content

#endif  // CONTENT_UTILITY_UTILITY_PROCESS_CONTROL_IMPL_H_
