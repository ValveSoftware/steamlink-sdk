// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/drive_metrics_provider.h"

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"

namespace metrics {

DriveMetricsProvider::DriveMetricsProvider(
    scoped_refptr<base::SequencedTaskRunner> file_thread,
    int local_state_path_key)
    : file_thread_(file_thread),
      local_state_path_key_(local_state_path_key),
      weak_ptr_factory_(this) {}

DriveMetricsProvider::~DriveMetricsProvider() {}

void DriveMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  auto* hardware = system_profile_proto->mutable_hardware();
  FillDriveMetrics(metrics_.app_drive, hardware->mutable_app_drive());
  FillDriveMetrics(metrics_.user_data_drive,
                   hardware->mutable_user_data_drive());
}

void DriveMetricsProvider::GetDriveMetrics(const base::Closure& done_callback) {
  base::PostTaskAndReplyWithResult(
      file_thread_.get(), FROM_HERE,
      base::Bind(&DriveMetricsProvider::GetDriveMetricsOnFileThread,
                 local_state_path_key_),
      base::Bind(&DriveMetricsProvider::GotDriveMetrics,
                 weak_ptr_factory_.GetWeakPtr(), done_callback));
}

DriveMetricsProvider::SeekPenaltyResponse::SeekPenaltyResponse()
    : success(false) {}

// static
DriveMetricsProvider::DriveMetrics
DriveMetricsProvider::GetDriveMetricsOnFileThread(int local_state_path_key) {
  DriveMetricsProvider::DriveMetrics metrics;
  QuerySeekPenalty(base::FILE_EXE, &metrics.app_drive);
  QuerySeekPenalty(local_state_path_key, &metrics.user_data_drive);
  return metrics;
}

// static
void DriveMetricsProvider::QuerySeekPenalty(
    int path_service_key,
    DriveMetricsProvider::SeekPenaltyResponse* response) {
  DCHECK(response);

  base::FilePath path;
  if (!PathService::Get(path_service_key, &path))
    return;

  base::TimeTicks start = base::TimeTicks::Now();

  response->success = HasSeekPenalty(path, &response->has_seek_penalty);

  UMA_HISTOGRAM_TIMES("Hardware.Drive.HasSeekPenalty_Time",
                      base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_BOOLEAN("Hardware.Drive.HasSeekPenalty_Success",
                        response->success);
  if (response->success) {
    UMA_HISTOGRAM_BOOLEAN("Hardware.Drive.HasSeekPenalty",
                          response->has_seek_penalty);
  }
}

void DriveMetricsProvider::GotDriveMetrics(
    const base::Closure& done_callback,
    const DriveMetricsProvider::DriveMetrics& metrics) {
  DCHECK(thread_checker_.CalledOnValidThread());
  metrics_ = metrics;
  done_callback.Run();
}

void DriveMetricsProvider::FillDriveMetrics(
    const DriveMetricsProvider::SeekPenaltyResponse& response,
    metrics::SystemProfileProto::Hardware::Drive* drive) {
  if (response.success)
    drive->set_has_seek_penalty(response.has_seek_penalty);
}

}  // namespace metrics
