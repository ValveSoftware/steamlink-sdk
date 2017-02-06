// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_POWER_TRACING_AGENT_H_
#define CONTENT_BROWSER_TRACING_POWER_TRACING_AGENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/threading/thread.h"
#include "base/trace_event/tracing_agent.h"
#include "content/public/browser/browser_thread.h"
#include "tools/battor_agent/battor_agent.h"
#include "tools/battor_agent/battor_error.h"

namespace base {
template <typename Type>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {

class PowerTracingAgent : public base::trace_event::TracingAgent,
                          public battor::BattOrAgent::Listener {
 public:
  // Retrieve the singleton instance.
  static PowerTracingAgent* GetInstance();

  void StartAgentTracing(const base::trace_event::TraceConfig& trace_config,
                         const StartAgentTracingCallback& callback) override;
  void StopAgentTracing(const StopAgentTracingCallback& callback) override;
  void RecordClockSyncMarker(
      const std::string& sync_id,
      const RecordClockSyncMarkerCallback& callback) override;

  bool SupportsExplicitClockSync() override;

  // base::trace_event::TracingAgent implementation.
  std::string GetTracingAgentName() override;
  std::string GetTraceEventLabel() override;

  // BattOrAgent::Listener implementation.
  void OnStartTracingComplete(battor::BattOrError error) override;
  void OnStopTracingComplete(const std::string& trace,
                             battor::BattOrError error) override;
  void OnRecordClockSyncMarkerComplete(battor::BattOrError error) override;

 private:
  // This allows constructor and destructor to be private and usable only
  // by the Singleton class.
  friend struct base::DefaultSingletonTraits<PowerTracingAgent>;

  PowerTracingAgent();
  ~PowerTracingAgent() override;

  void FindBattOrOnFileThread(const StartAgentTracingCallback& callback);
  void StartAgentTracingOnIOThread(const std::string& path,
                                   const StartAgentTracingCallback& callback);
  void StopAgentTracingOnIOThread(const StopAgentTracingCallback& callback);
  void RecordClockSyncMarkerOnIOThread(
      const std::string& sync_id,
      const RecordClockSyncMarkerCallback& callback);

  // Returns the path of a BattOr (e.g. /dev/ttyUSB0), or an empty string if
  // none are found.
  std::string GetBattOrPath();

  // All interactions with the BattOrAgent (after construction) must happen on
  // the IO thread.
  std::unique_ptr<battor::BattOrAgent, BrowserThread::DeleteOnIOThread>
      battor_agent_;

  StartAgentTracingCallback start_tracing_callback_;
  StopAgentTracingCallback stop_tracing_callback_;
  std::string record_clock_sync_marker_sync_id_;
  base::TimeTicks record_clock_sync_marker_start_time_;
  RecordClockSyncMarkerCallback record_clock_sync_marker_callback_;

  DISALLOW_COPY_AND_ASSIGN(PowerTracingAgent);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_POWER_TRACING_AGENT_H_
