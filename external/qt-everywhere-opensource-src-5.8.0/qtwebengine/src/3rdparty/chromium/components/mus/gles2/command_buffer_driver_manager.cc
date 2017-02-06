// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/command_buffer_driver_manager.h"

#include <algorithm>

#include "components/mus/gles2/command_buffer_driver.h"

namespace mus {

CommandBufferDriverManager::CommandBufferDriverManager() {}

CommandBufferDriverManager::~CommandBufferDriverManager() {}

void CommandBufferDriverManager::AddDriver(CommandBufferDriver* driver) {
  DCHECK(CalledOnValidThread());
  DCHECK(std::find(drivers_.begin(), drivers_.end(), driver) == drivers_.end());
  drivers_.push_back(driver);
}

void CommandBufferDriverManager::RemoveDriver(CommandBufferDriver* driver) {
  DCHECK(CalledOnValidThread());
  auto it = std::find(drivers_.begin(), drivers_.end(), driver);
  DCHECK(it != drivers_.end());
  drivers_.erase(it);
}

uint32_t CommandBufferDriverManager::GetUnprocessedOrderNum() const {
  DCHECK(CalledOnValidThread());
  uint32_t unprocessed_order_num = 0;
  for (auto& d : drivers_) {
    unprocessed_order_num =
      std::max(unprocessed_order_num, d->GetUnprocessedOrderNum());
  }
  return unprocessed_order_num;
}

uint32_t CommandBufferDriverManager::GetProcessedOrderNum() const {
  DCHECK(CalledOnValidThread());
  uint32_t processed_order_num = 0;
  for (auto& d : drivers_) {
    processed_order_num =
        std::max(processed_order_num, d->GetProcessedOrderNum());
  }
  return processed_order_num;
}

}  // namespace mus
