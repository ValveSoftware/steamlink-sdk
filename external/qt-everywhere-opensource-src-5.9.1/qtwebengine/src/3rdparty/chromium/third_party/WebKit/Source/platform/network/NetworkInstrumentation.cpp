// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/NetworkInstrumentation.h"

#include "base/trace_event/trace_event.h"
#include "platform/network/ResourceLoadPriority.h"
#include "platform/network/ResourceRequest.h"
#include "platform/tracing/TracedValue.h"

namespace network_instrumentation {

using network_instrumentation::RequestOutcome;
using blink::TracedValue;

const char kBlinkResourceID[] = "BlinkResourceID";
const char kResourceLoadTitle[] = "ResourceLoad";
const char kResourcePrioritySetTitle[] = "ResourcePrioritySet";
const char kNetInstrumentationCategory[] = TRACE_DISABLED_BY_DEFAULT("network");

const char* RequestOutcomeToString(RequestOutcome outcome) {
  switch (outcome) {
    case RequestOutcome::Success:
      return "Success";
    case RequestOutcome::Fail:
      return "Fail";
    default:
      NOTREACHED();
      // We need to return something to avoid compiler warning.
      return "This should never happen";
  }
}

// Note: network_instrumentation code should do as much work as possible inside
// the arguments of trace macros so that very little instrumentation overhead is
// incurred if the trace category is disabled. See https://crbug.com/669666.

namespace {

std::unique_ptr<TracedValue> scopedResourceTrackerBeginData(
    const blink::ResourceRequest& request) {
  std::unique_ptr<TracedValue> data = TracedValue::create();
  data->setString("url", request.url().getString());
  return data;
}

std::unique_ptr<TracedValue> resourcePrioritySetData(
    blink::ResourceLoadPriority priority) {
  std::unique_ptr<TracedValue> data = TracedValue::create();
  data->setInteger("priority", priority);
  return data;
}

std::unique_ptr<TracedValue> endResourceLoadData(RequestOutcome outcome) {
  std::unique_ptr<TracedValue> data = TracedValue::create();
  data->setString("outcome", RequestOutcomeToString(outcome));
  return data;
}

}  // namespace

ScopedResourceLoadTracker::ScopedResourceLoadTracker(
    unsigned long resourceID,
    const blink::ResourceRequest& request)
    : m_resourceLoadContinuesBeyondScope(false), m_resourceID(resourceID) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      kNetInstrumentationCategory, kResourceLoadTitle,
      TRACE_ID_WITH_SCOPE(kBlinkResourceID, TRACE_ID_LOCAL(resourceID)),
      "beginData", scopedResourceTrackerBeginData(request));
}

ScopedResourceLoadTracker::~ScopedResourceLoadTracker() {
  if (!m_resourceLoadContinuesBeyondScope)
    endResourceLoad(m_resourceID, RequestOutcome::Fail);
}

void ScopedResourceLoadTracker::resourceLoadContinuesBeyondScope() {
  m_resourceLoadContinuesBeyondScope = true;
}

void resourcePrioritySet(unsigned long resourceID,
                         blink::ResourceLoadPriority priority) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(
      kNetInstrumentationCategory, kResourcePrioritySetTitle,
      TRACE_ID_WITH_SCOPE(kBlinkResourceID, TRACE_ID_LOCAL(resourceID)), "data",
      resourcePrioritySetData(priority));
}

void endResourceLoad(unsigned long resourceID, RequestOutcome outcome) {
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      kNetInstrumentationCategory, kResourceLoadTitle,
      TRACE_ID_WITH_SCOPE(kBlinkResourceID, TRACE_ID_LOCAL(resourceID)),
      "endData", endResourceLoadData(outcome));
}

}  // namespace network_instrumentation
