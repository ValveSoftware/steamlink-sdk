// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GLES2_COMMAND_BUFFER_DRIVER_MANAGER_H_
#define COMPONENTS_MUS_GLES2_COMMAND_BUFFER_DRIVER_MANAGER_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "base/threading/non_thread_safe.h"

namespace mus {

class CommandBufferDriver;

// This class manages all initialized |CommandBufferDriver|s.
class CommandBufferDriverManager : base::NonThreadSafe {
 public:
  CommandBufferDriverManager();
  ~CommandBufferDriverManager();

  // Add a new initialized driver to the manager.
  void AddDriver(CommandBufferDriver* driver);

  // Remove a driver from the manager.
  void RemoveDriver(CommandBufferDriver* driver);

  // Return the global order number for the last unprocessed flush
  // (|CommandBufferDriver::Flush|).
  uint32_t GetUnprocessedOrderNum() const;

  // Return the global order number for the last processed flush
  // (|CommandBufferDriver::Flush|).
  uint32_t GetProcessedOrderNum() const;

 private:
  std::vector<CommandBufferDriver*> drivers_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferDriverManager);
};

}  // namespace mus

#endif  // COMPONENTS_GLES2_COMMAND_BUFFER_DRIVER_MANAGER_H_
