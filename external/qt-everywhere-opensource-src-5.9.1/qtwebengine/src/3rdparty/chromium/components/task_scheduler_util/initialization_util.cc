// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/initialization_util.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/task_scheduler/initialization_util.h"
#include "base/task_scheduler/scheduler_worker_pool_params.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/task_scheduler/task_traits.h"
#include "base/time/time.h"

namespace task_scheduler_util {

namespace {

using StandbyThreadPolicy =
    base::SchedulerWorkerPoolParams::StandbyThreadPolicy;

enum WorkerPoolType : size_t {
  BACKGROUND_WORKER_POOL = 0,
  BACKGROUND_FILE_IO_WORKER_POOL,
  FOREGROUND_WORKER_POOL,
  FOREGROUND_FILE_IO_WORKER_POOL,
  WORKER_POOL_COUNT  // Always last.
};

struct WorkerPoolVariationValues {
  StandbyThreadPolicy standby_thread_policy;
  int threads = 0;
  base::TimeDelta detach_period;
};

// Converts |pool_descriptor| to a WorkerPoolVariationValues. Returns a default
// WorkerPoolVariationValues on failure.
//
// |pool_descriptor| is a semi-colon separated value string with the following
// items:
// 0. Minimum Thread Count (int)
// 1. Maximum Thread Count (int)
// 2. Thread Count Multiplier (double)
// 3. Thread Count Offset (int)
// 4. Detach Time in Milliseconds (milliseconds)
// 5. Standby Thread Policy (string)
// Additional values may appear as necessary and will be ignored.
WorkerPoolVariationValues StringToWorkerPoolVariationValues(
    const base::StringPiece pool_descriptor) {
  const std::vector<std::string> tokens =
      SplitString(pool_descriptor, ";",
                  base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int min;
  int max;
  double cores_multiplier;
  int offset;
  int detach_milliseconds;
  // Checking for a size greater than the expected amount allows us to be
  // forward compatible if we add more variation values.
  if (tokens.size() >= 5 && base::StringToInt(tokens[0], &min) &&
      base::StringToInt(tokens[1], &max) &&
      base::StringToDouble(tokens[2], &cores_multiplier) &&
      base::StringToInt(tokens[3], &offset) &&
      base::StringToInt(tokens[4], &detach_milliseconds)) {
    WorkerPoolVariationValues values;
    values.threads = base::RecommendedMaxNumberOfThreadsInPool(
        min, max, cores_multiplier, offset);
    values.detach_period =
        base::TimeDelta::FromMilliseconds(detach_milliseconds);
    values.standby_thread_policy =
        (tokens.size() >= 6 && tokens[5] == "lazy")
            ? StandbyThreadPolicy::LAZY
            : StandbyThreadPolicy::ONE;
    return values;
  }
  DLOG(ERROR) << "Invalid Worker Pool Descriptor: " << pool_descriptor;
  return WorkerPoolVariationValues();
}

// Returns the worker pool index for |traits| defaulting to
// FOREGROUND_WORKER_POOL or FOREGROUND_FILE_IO_WORKER_POOL on unknown
// priorities.
size_t WorkerPoolIndexForTraits(const base::TaskTraits& traits) {
  const bool is_background =
      traits.priority() == base::TaskPriority::BACKGROUND;
  if (traits.with_file_io()) {
    return is_background ? BACKGROUND_FILE_IO_WORKER_POOL
                         : FOREGROUND_FILE_IO_WORKER_POOL;
  }
  return is_background ? BACKGROUND_WORKER_POOL : FOREGROUND_WORKER_POOL;
}

}  // namespace

bool InitializeDefaultTaskScheduler(
    const std::map<std::string, std::string>& variation_params) {
  using ThreadPriority = base::ThreadPriority;
  using IORestriction = base::SchedulerWorkerPoolParams::IORestriction;
  struct SchedulerWorkerPoolPredefinedParams {
    const char* name;
    ThreadPriority priority_hint;
    IORestriction io_restriction;
  };
  static const SchedulerWorkerPoolPredefinedParams kAllPredefinedParams[] = {
    {"Background", ThreadPriority::BACKGROUND, IORestriction::DISALLOWED},
    {"BackgroundFileIO", ThreadPriority::BACKGROUND, IORestriction::ALLOWED},
    {"Foreground", ThreadPriority::NORMAL, IORestriction::DISALLOWED},
    {"ForegroundFileIO", ThreadPriority::NORMAL, IORestriction::ALLOWED},
  };
  static_assert(arraysize(kAllPredefinedParams) == WORKER_POOL_COUNT,
                "Mismatched Worker Pool Types and Predefined Parameters");

  std::vector<base::SchedulerWorkerPoolParams> params_vector;
  for (const auto& predefined_params : kAllPredefinedParams) {
    const auto pair = variation_params.find(predefined_params.name);
    if (pair == variation_params.end()) {
      DLOG(ERROR) << "Missing Worker Pool Configuration: "
                  << predefined_params.name;
      return false;
    }

    const WorkerPoolVariationValues variation_values =
        StringToWorkerPoolVariationValues(pair->second);

    if (variation_values.threads <= 0 ||
        variation_values.detach_period.is_zero()) {
      DLOG(ERROR) << "Invalid Worker Pool Configuration: " <<
          predefined_params.name << " [" << pair->second << "]";
      return false;
    }

    params_vector.emplace_back(predefined_params.name,
                               predefined_params.priority_hint,
                               predefined_params.io_restriction,
                               variation_values.standby_thread_policy,
                               variation_values.threads,
                               variation_values.detach_period);
  }

  DCHECK_EQ(WORKER_POOL_COUNT, params_vector.size());

  base::TaskScheduler::CreateAndSetDefaultTaskScheduler(
      params_vector, base::Bind(WorkerPoolIndexForTraits));

  return true;
}

}  // namespace task_scheduler_util
