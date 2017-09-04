// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/profiler/content/content_tracking_synchronizer_delegate.h"

#include "base/memory/ptr_util.h"
#include "components/metrics/profiler/tracking_synchronizer.h"
#include "components/nacl/common/nacl_process_type.h"
#include "content/public/browser/profiler_controller.h"
#include "content/public/common/process_type.h"

namespace metrics {

namespace {

ProfilerEventProto::TrackedObject::ProcessType AsProtobufProcessType(
    int process_type) {
  switch (process_type) {
    case content::PROCESS_TYPE_BROWSER:
      return ProfilerEventProto::TrackedObject::BROWSER;
    case content::PROCESS_TYPE_RENDERER:
      return ProfilerEventProto::TrackedObject::RENDERER;
    case content::PROCESS_TYPE_UTILITY:
      return ProfilerEventProto::TrackedObject::UTILITY;
    case content::PROCESS_TYPE_ZYGOTE:
      return ProfilerEventProto::TrackedObject::ZYGOTE;
    case content::PROCESS_TYPE_SANDBOX_HELPER:
      return ProfilerEventProto::TrackedObject::SANDBOX_HELPER;
    case content::PROCESS_TYPE_GPU:
      return ProfilerEventProto::TrackedObject::GPU;
    case content::PROCESS_TYPE_PPAPI_PLUGIN:
      return ProfilerEventProto::TrackedObject::PPAPI_PLUGIN;
    case content::PROCESS_TYPE_PPAPI_BROKER:
      return ProfilerEventProto::TrackedObject::PPAPI_BROKER;
    case PROCESS_TYPE_NACL_LOADER:
      return ProfilerEventProto::TrackedObject::NACL_LOADER;
    case PROCESS_TYPE_NACL_BROKER:
      return ProfilerEventProto::TrackedObject::NACL_BROKER;
    default:
      NOTREACHED();
      return ProfilerEventProto::TrackedObject::UNKNOWN;
  }
}

}  // namespace

// static
std::unique_ptr<TrackingSynchronizerDelegate>
ContentTrackingSynchronizerDelegate::Create(
    TrackingSynchronizer* synchronizer) {
  return base::WrapUnique(
      new ContentTrackingSynchronizerDelegate(synchronizer));
}

ContentTrackingSynchronizerDelegate::ContentTrackingSynchronizerDelegate(
    TrackingSynchronizer* synchronizer)
    : synchronizer_(synchronizer) {
  DCHECK(synchronizer_);
  content::ProfilerController::GetInstance()->Register(this);
}

ContentTrackingSynchronizerDelegate::~ContentTrackingSynchronizerDelegate() {
  content::ProfilerController::GetInstance()->Unregister(this);
}

void ContentTrackingSynchronizerDelegate::GetProfilerDataForChildProcesses(
    int sequence_number,
    int current_profiling_phase) {
  // Get profiler data from renderer and browser child processes.
  content::ProfilerController::GetInstance()->GetProfilerData(
      sequence_number, current_profiling_phase);
}

void ContentTrackingSynchronizerDelegate::OnProfilingPhaseCompleted(
    int profiling_phase) {
  // Notify renderer and browser child processes.
  content::ProfilerController::GetInstance()->OnProfilingPhaseCompleted(
      profiling_phase);
}

void ContentTrackingSynchronizerDelegate::OnPendingProcesses(
    int sequence_number,
    int pending_processes,
    bool end) {
  // Notify |synchronizer_|.
  synchronizer_->OnPendingProcesses(sequence_number, pending_processes, end);
}

void ContentTrackingSynchronizerDelegate::OnProfilerDataCollected(
    int sequence_number,
    const tracked_objects::ProcessDataSnapshot& profiler_data,
    content::ProcessType process_type) {
  // Notify |synchronizer_|.
  synchronizer_->OnProfilerDataCollected(sequence_number, profiler_data,
                                         AsProtobufProcessType(process_type));
}

}  // namespace metrics
