// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/histogram_message_filter.h"

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "content/browser/histogram_controller.h"
#include "content/common/child_process_messages.h"
#include "content/public/common/content_switches.h"

namespace content {

HistogramMessageFilter::HistogramMessageFilter()
    : BrowserMessageFilter(ChildProcessMsgStart) {}

bool HistogramMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(HistogramMessageFilter, message)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_ChildHistogramData,
                        OnChildHistogramData)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_GetBrowserHistogram,
                        OnGetBrowserHistogram)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

HistogramMessageFilter::~HistogramMessageFilter() {}

void HistogramMessageFilter::OnChildHistogramData(
    int sequence_number,
    const std::vector<std::string>& pickled_histograms) {
  HistogramController::GetInstance()->OnHistogramDataCollected(
      sequence_number, pickled_histograms);
}

void HistogramMessageFilter::OnGetBrowserHistogram(
    const std::string& name,
    std::string* histogram_json) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // Security: Only allow access to browser histograms when running in the
  // context of a test.
  bool using_stats_collection_controller =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kStatsCollectionController);
  if (!using_stats_collection_controller) {
    LOG(ERROR) << "Attempt at reading browser histogram without specifying "
               << "--" << switches::kStatsCollectionController << " switch.";
    return;
  }
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(name);
  if (!histogram) {
    *histogram_json = "{}";
  } else {
    histogram->WriteJSON(histogram_json);
  }
}

}  // namespace content
