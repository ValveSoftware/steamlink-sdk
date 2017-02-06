// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bad_message.h"

#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/render_process_host.h"

namespace content {
namespace bad_message {

namespace {

void LogBadMessage(BadMessageReason reason) {
  LOG(ERROR) << "Terminating renderer for bad IPC message, reason " << reason;
  UMA_HISTOGRAM_SPARSE_SLOWLY("Stability.BadMessageTerminated.Content", reason);
  base::debug::SetCrashKeyValue("bad_message_reason",
                                base::IntToString(reason));
}

}  // namespace

void ReceivedBadMessage(RenderProcessHost* host, BadMessageReason reason) {
  LogBadMessage(reason);
  host->ShutdownForBadMessage();
}

void ReceivedBadMessage(BrowserMessageFilter* filter, BadMessageReason reason) {
  LogBadMessage(reason);
  filter->ShutdownForBadMessage();
}

}  // namespace bad_message
}  // namespace content
