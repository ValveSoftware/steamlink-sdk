// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/metrics/oom_kills_monitor.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/posix/safe_strerror.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "components/arc/metrics/oom_kills_histogram.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/re2/src/re2/stringpiece.h"

namespace arc {

using base::StringPiece;

using base::SequencedWorkerPool;
using base::TimeDelta;

OomKillsMonitor::OomKillsMonitor()
    : worker_pool_(
          new SequencedWorkerPool(1, "oom_kills_monitor")) {}

OomKillsMonitor::~OomKillsMonitor() {
  Stop();
}

void OomKillsMonitor::Start() {
  auto task_runner = worker_pool_->GetTaskRunnerWithShutdownBehavior(
      SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
  task_runner->PostTask(
      FROM_HERE, base::Bind(&OomKillsMonitor::Run, worker_pool_));
}

void OomKillsMonitor::Stop() {
  worker_pool_->Shutdown();
}

namespace {

int64_t GetTimestamp(const StringPiece& line) {
  std::vector<StringPiece> fields = base::SplitStringPiece(
      line, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  int64_t timestamp = -1;
  // Timestamp is the third field in a line of /dev/kmsg.
  if (fields.size() < 3 || !base::StringToInt64(fields[2], &timestamp))
    return -1;
  return timestamp;
}

bool GetTimeDelta(
    const StringPiece& line, int64_t* last, TimeDelta* time_delta) {
  int64_t now = GetTimestamp(line);
  if (now < 0)
    return false;

  // Sets to |kMaxOomMemoryKillTimeSecs| for the first kill event.
  if (*last < 0)
    *time_delta = TimeDelta::FromSeconds(kMaxOomMemoryKillTimeDeltaSecs);
  else
    *time_delta = TimeDelta::FromMicroseconds(now - *last);

  *last = now;

  return true;
}

void LogOOMKill(const StringPiece& line) {
  static int64_t last_timestamp = -1;
  static int oom_kills = 0;

  // Sample log line:
  // 3,1362,97646497541,-;Out of memory: Kill process 29582 (android.vending)
  // score 961 or sacrifice child.
  int oom_badness;
  TimeDelta time_delta;
  if (RE2::PartialMatch(re2::StringPiece(line.data(), line.size()),
                        "Out of memory: Kill process .* score (\\d+)",
                        &oom_badness)) {
    ++oom_kills;
    // Report the cumulative count of killed process in one login session.
    // For example if there are 3 processes killed, it would report 1 for the
    // first kill, 2 for the second kill, then 3 for the final kill.
    // It doesn't report a final count at the end of a user session because
    // the code runs in a dedicated thread and never ends until browser shutdown
    // (or logout on Chrome OS). And on browser shutdown the thread may be
    // terminated brutally so there's no chance to execute a "final" block.
    // More specifically, code outside the main loop of OomKillsMonitor::Run()
    // are not guaranteed to be executed.
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Arc.OOMKills.Count", oom_kills, 1, 1000, 1001);

    // In practice most process has oom_badness < 1000, but
    // strictly speaking the number could be [1, 2000]. What it really
    // means is the baseline, proportion of memory used (normalized to
    // [0, 1000]), plus an adjustment score oom_score_adj [-1000, 1000],
    // truncated to 1 if negative (0 means never kill).
    // Ref: https://lwn.net/Articles/396552/
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Arc.OOMKills.Score", oom_badness, 1, 2000, 2001);

    if (GetTimeDelta(line, &last_timestamp, &time_delta)) {
      UMA_HISTOGRAM_OOM_KILL_TIME_INTERVAL(
          "Arc.OOMKills.TimeDelta", time_delta);
    }
  }
}

void LogLowMemoryKill(const StringPiece& line) {
  static int64_t last_timestamp = -1;
  static int low_memory_kills = 0;

  int freed_size;
  TimeDelta time_delta;
  // Sample log line:
  // 6,2302,533604004,-;lowmemorykiller: Killing 'externalstorage' (21742),
  // adj 1000,\x0a   to free 27320kB on behalf of 'kswapd0' (47) because\x0a
  // cache 181920kB is below limit 184320kB for oom_score_adj 1000\x0a
  // Free memory is 1228kB above reserved
  if (RE2::PartialMatch(re2::StringPiece(line.data(), line.size()),
                        "lowmemorykiller: .* to free (\\d+)kB",
                        &freed_size)) {
    // Report the count for each lowmemorykill event. See comments in
    // LogOOMKill().
    ++low_memory_kills;
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Arc.LowMemoryKiller.Count", low_memory_kills, 1, 1000, 1001);

    UMA_HISTOGRAM_MEMORY_KB("Arc.LowMemoryKiller.FreedSize", freed_size);

    if(GetTimeDelta(line, &last_timestamp, &time_delta)) {
      UMA_HISTOGRAM_OOM_KILL_TIME_INTERVAL(
          "Arc.LowMemoryKiller.TimeDelta", time_delta);
    }
  }
}

}  // namespace

// static
void OomKillsMonitor::Run(
    scoped_refptr<base::SequencedWorkerPool> worker_pool) {
  base::ScopedFILE kmsg_handle(
      base::OpenFile(base::FilePath("/dev/kmsg"), "r"));
  if (!kmsg_handle) {
    LOG(WARNING) << "Open /dev/kmsg failed: " << base::safe_strerror(errno);
    return;
  }
  // Skip kernel messages prior to the instantiation of this object to avoid
  // double reporting.
  fseek(kmsg_handle.get(), 0, SEEK_END);

  static const int kMaxBufSize = 512;
  char buf[kMaxBufSize];

  while (fgets(buf, kMaxBufSize, kmsg_handle.get())) {
    if (worker_pool->IsShutdownInProgress()) {
      DVLOG(1) << "Chrome is shutting down, exit now.";
      break;
    }
    const StringPiece buf_string(buf);
    LogOOMKill(buf_string);
    LogLowMemoryKill(buf_string);
  }
}

}  // namespace arc
