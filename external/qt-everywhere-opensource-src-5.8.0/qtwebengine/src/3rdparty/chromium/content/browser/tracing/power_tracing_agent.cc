// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event_impl.h"
#include "content/browser/tracing/power_tracing_agent.h"
#include "tools/battor_agent/battor_finder.h"

namespace content {

namespace {

const char kPowerTracingAgentName[] = "battor";
const char kPowerTraceLabel[] = "powerTraceAsString";

}  // namespace

// static
PowerTracingAgent* PowerTracingAgent::GetInstance() {
  return base::Singleton<PowerTracingAgent>::get();
}

PowerTracingAgent::PowerTracingAgent() {}
PowerTracingAgent::~PowerTracingAgent() {}

std::string PowerTracingAgent::GetTracingAgentName() {
  return kPowerTracingAgentName;
}

std::string PowerTracingAgent::GetTraceEventLabel() {
  return kPowerTraceLabel;
}

void PowerTracingAgent::StartAgentTracing(
    const base::trace_event::TraceConfig& trace_config,
    const StartAgentTracingCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&PowerTracingAgent::FindBattOrOnFileThread,
                                     base::Unretained(this), callback));
}

void PowerTracingAgent::FindBattOrOnFileThread(
    const StartAgentTracingCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  std::string path = battor::BattOrFinder::FindBattOr();
  if (path.empty()) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(callback, GetTracingAgentName(), false /* success */));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PowerTracingAgent::StartAgentTracingOnIOThread,
                 base::Unretained(this), path, callback));
}

void PowerTracingAgent::StartAgentTracingOnIOThread(
    const std::string& path,
    const StartAgentTracingCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  battor_agent_.reset(new battor::BattOrAgent(
      path, this,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI)));

  start_tracing_callback_ = callback;
  battor_agent_->StartTracing();
}

void PowerTracingAgent::OnStartTracingComplete(battor::BattOrError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  bool success = (error == battor::BATTOR_ERROR_NONE);
  if (!success)
    battor_agent_.reset();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(start_tracing_callback_, GetTracingAgentName(), success));
  start_tracing_callback_.Reset();
}

void PowerTracingAgent::StopAgentTracing(
    const StopAgentTracingCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PowerTracingAgent::StopAgentTracingOnIOThread,
                 base::Unretained(this), callback));
}

void PowerTracingAgent::StopAgentTracingOnIOThread(
    const StopAgentTracingCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!battor_agent_) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(callback, GetTracingAgentName(), GetTraceEventLabel(),
                   nullptr /* events_str_ptr */));
    return;
  }

  stop_tracing_callback_ = callback;
  battor_agent_->StopTracing();
}

void PowerTracingAgent::OnStopTracingComplete(const std::string& trace,
                                              battor::BattOrError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  scoped_refptr<base::RefCountedString> result(new base::RefCountedString());
  if (error == battor::BATTOR_ERROR_NONE)
    result->data() = trace;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(stop_tracing_callback_, GetTracingAgentName(),
                 GetTraceEventLabel(), result));
  stop_tracing_callback_.Reset();
  battor_agent_.reset();
}

void PowerTracingAgent::RecordClockSyncMarker(
    const std::string& sync_id,
    const RecordClockSyncMarkerCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(SupportsExplicitClockSync());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PowerTracingAgent::RecordClockSyncMarkerOnIOThread,
                 base::Unretained(this), sync_id, callback));
}

void PowerTracingAgent::RecordClockSyncMarkerOnIOThread(
    const std::string& sync_id,
    const RecordClockSyncMarkerCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(battor_agent_);

  record_clock_sync_marker_sync_id_ = sync_id;
  record_clock_sync_marker_callback_ = callback;
  record_clock_sync_marker_start_time_ = base::TimeTicks::Now();
  battor_agent_->RecordClockSyncMarker(sync_id);
}

void PowerTracingAgent::OnRecordClockSyncMarkerComplete(
    battor::BattOrError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::TimeTicks issue_start_ts = record_clock_sync_marker_start_time_;
  base::TimeTicks issue_end_ts = base::TimeTicks::Now();

  if (error != battor::BATTOR_ERROR_NONE)
    issue_start_ts = issue_end_ts = base::TimeTicks();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(record_clock_sync_marker_callback_,
                 record_clock_sync_marker_sync_id_,
                 issue_start_ts,
                 issue_end_ts));

  record_clock_sync_marker_callback_.Reset();
  record_clock_sync_marker_sync_id_ = std::string();
  record_clock_sync_marker_start_time_ = base::TimeTicks();
}

bool PowerTracingAgent::SupportsExplicitClockSync() {
  return battor_agent_->SupportsExplicitClockSync();
}

}  // namespace content
