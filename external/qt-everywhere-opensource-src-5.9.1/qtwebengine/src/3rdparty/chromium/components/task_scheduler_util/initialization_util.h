// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TASK_SCHEDULER_UTIL_INITIALIZATION_UTIL_H_
#define COMPONENTS_TASK_SCHEDULER_UTIL_INITIALIZATION_UTIL_H_

#include <map>
#include <string>

namespace task_scheduler_util {

// Calls base::TaskScheduler::CreateAndSetDefaultTaskScheduler with arguments
// derived from |variation_params|. Returns false on failure. |variation_params|
// is expected to come from the variations component and map a thread pool name
// to thread pool parameters.
bool InitializeDefaultTaskScheduler(
    const std::map<std::string, std::string>& variation_params);

}  // namespace task_scheduler_util

#endif  // BASE_TASK_SCHEDULER_INITIALIZATION_UTIL_H_
