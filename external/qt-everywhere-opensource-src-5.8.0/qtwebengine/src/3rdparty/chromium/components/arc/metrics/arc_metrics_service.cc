// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/metrics/arc_metrics_service.h"

#include <string>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"

namespace {

const int kRequestProcessListPeriodInMinutes = 5;
const char kArcProcessNamePrefix[] = "org.chromium.arc.";
const char kGmsProcessNamePrefix[] = "com.google.android.gms";
const char kBootProgressEnableScreen[] = "boot_progress_enable_screen";

}  // namespace

namespace arc {

ArcMetricsService::ArcMetricsService(ArcBridgeService* bridge_service)
    : ArcService(bridge_service),
      binding_(this),
      process_observer_(this),
      weak_ptr_factory_(this) {
  arc_bridge_service()->metrics()->AddObserver(this);
  arc_bridge_service()->process()->AddObserver(&process_observer_);
  oom_kills_monitor_.Start();
}

ArcMetricsService::~ArcMetricsService() {
  DCHECK(CalledOnValidThread());
  arc_bridge_service()->process()->RemoveObserver(&process_observer_);
  arc_bridge_service()->metrics()->RemoveObserver(this);
}

bool ArcMetricsService::CalledOnValidThread() {
  // Make sure access to the Chrome clipboard is happening in the UI thread.
  return thread_checker_.CalledOnValidThread();
}

void ArcMetricsService::OnInstanceReady() {
  VLOG(2) << "Start metrics service.";
  // Retrieve ARC start time from session manager.
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->GetArcStartTime(
      base::Bind(&ArcMetricsService::OnArcStartTimeRetrieved,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ArcMetricsService::OnInstanceClosed() {
  VLOG(2) << "Close metrics service.";
  DCHECK(CalledOnValidThread());
  if (binding_.is_bound())
    binding_.Unbind();
}

void ArcMetricsService::OnProcessInstanceReady() {
  VLOG(2) << "Start updating process list.";
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMinutes(kRequestProcessListPeriodInMinutes),
               this, &ArcMetricsService::RequestProcessList);
}

void ArcMetricsService::OnProcessInstanceClosed() {
  VLOG(2) << "Stop updating process list.";
  timer_.Stop();
}

void ArcMetricsService::RequestProcessList() {
  mojom::ProcessInstance* process_instance =
      arc_bridge_service()->process()->instance();
  if (!process_instance) {
    LOG(ERROR) << "No process instance found before RequestProcessList";
    return;
  }

  VLOG(2) << "RequestProcessList";
  process_instance->RequestProcessList(base::Bind(
      &ArcMetricsService::ParseProcessList, weak_ptr_factory_.GetWeakPtr()));
}

void ArcMetricsService::ParseProcessList(
    mojo::Array<arc::mojom::RunningAppProcessInfoPtr> processes) {
  int running_app_count = 0;
  for (const auto& process : processes) {
    const mojo::String& process_name = process->process_name;
    const mojom::ProcessState& process_state = process->process_state;

    // Processes like the ARC launcher and intent helper are always running
    // and not counted as apps running by users. With the same reasoning,
    // GMS (Google Play Services) and its related processes are skipped as
    // well. The process_state check below filters out system processes,
    // services, apps that are cached because they've run before.
    if (base::StartsWith(process_name.get(), kArcProcessNamePrefix,
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(process_name.get(), kGmsProcessNamePrefix,
                         base::CompareCase::SENSITIVE) ||
        process_state != mojom::ProcessState::TOP) {
      VLOG(2) << "Skipped " << process_name << " " << process_state;
    } else {
      ++running_app_count;
    }
  }

  UMA_HISTOGRAM_COUNTS_100("Arc.AppCount", running_app_count);
}

void ArcMetricsService::OnArcStartTimeRetrieved(
    bool success,
    base::TimeTicks arc_start_time) {
  DCHECK(CalledOnValidThread());
  if (!success) {
    LOG(ERROR) << "Failed to retrieve ARC start timeticks.";
    return;
  }
  if (!arc_bridge_service()->metrics()->instance()) {
    LOG(ERROR) << "ARC metrics instance went away while retrieving start time.";
    return;
  }

  // The binding of host interface is deferred until the ARC start time is
  // retrieved here because it prevents race condition of the ARC start
  // time availability in ReportBootProgress().
  if (!binding_.is_bound()) {
    mojom::MetricsHostPtr host_ptr;
    binding_.Bind(mojo::GetProxy(&host_ptr));
    arc_bridge_service()->metrics()->instance()->Init(std::move(host_ptr));
  }
  arc_start_time_ = arc_start_time;
  VLOG(2) << "ARC start @" << arc_start_time_;
}

void ArcMetricsService::ReportBootProgress(
    mojo::Array<arc::mojom::BootProgressEventPtr> events) {
  DCHECK(CalledOnValidThread());
  int64_t arc_start_time_in_ms =
      (arc_start_time_ - base::TimeTicks()).InMilliseconds();
  for (const auto& event : events) {
    VLOG(2) << "Report boot progress event:" << event->event << "@"
            << event->uptimeMillis;
    std::string title = "Arc." + event->event.get();
    base::TimeDelta elapsed_time = base::TimeDelta::FromMilliseconds(
        event->uptimeMillis - arc_start_time_in_ms);
    // Note: This leaks memory, which is expected behavior.
    base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
        title, base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromSeconds(30), 50,
        base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->AddTime(elapsed_time);
    if (event->event.get().compare(kBootProgressEnableScreen) == 0)
      UMA_HISTOGRAM_CUSTOM_TIMES("Arc.AndroidBootTime", elapsed_time,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromSeconds(30), 50);
  }
}

ArcMetricsService::ProcessObserver::ProcessObserver(
    ArcMetricsService* arc_metrics_service)
    : arc_metrics_service_(arc_metrics_service) {}

ArcMetricsService::ProcessObserver::~ProcessObserver() = default;

void ArcMetricsService::ProcessObserver::OnInstanceReady() {
  arc_metrics_service_->OnProcessInstanceReady();
}

void ArcMetricsService::ProcessObserver::OnInstanceClosed() {
  arc_metrics_service_->OnProcessInstanceClosed();
}

}  // namespace arc
