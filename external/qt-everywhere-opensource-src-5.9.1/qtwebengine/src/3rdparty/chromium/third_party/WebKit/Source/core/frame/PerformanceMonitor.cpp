// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/PerformanceMonitor.h"

#include "bindings/core/v8/ScheduledAction.h"
#include "bindings/core/v8/ScriptEventListener.h"
#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventListener.h"
#include "core/frame/Frame.h"
#include "core/frame/LocalFrame.h"
#include "core/html/parser/HTMLDocumentParser.h"
#include "public/platform/Platform.h"
#include "wtf/CurrentTime.h"

namespace blink {

PerformanceMonitor::HandlerCall::HandlerCall(ExecutionContext* context,
                                             EventListener* eventListener)
    : m_context(context),
      m_performanceMonitor(PerformanceMonitor::instrumentingMonitor(context)),
      m_eventListener(eventListener),
      m_violation(kHandler) {
  start();
}

PerformanceMonitor::HandlerCall::HandlerCall(ExecutionContext* context,
                                             ScheduledAction* scheduledAction)
    : m_context(context),
      m_performanceMonitor(PerformanceMonitor::instrumentingMonitor(context)),
      m_scheduledAction(scheduledAction),
      m_violation(kRecurringHandler) {
  start();
}

void PerformanceMonitor::HandlerCall::start() {
  if (!m_performanceMonitor || !m_performanceMonitor->m_thresholds[m_violation])
    return;
  m_startTime = WTF::monotonicallyIncreasingTime();
}

PerformanceMonitor::HandlerCall::~HandlerCall() {
  if (!m_performanceMonitor)
    return;
  double threshold = m_performanceMonitor->m_thresholds[m_violation];
  if (!threshold)
    return;
  double time = WTF::monotonicallyIncreasingTime() - m_startTime;
  if (time < threshold)
    return;

  String text = String::format("Handler took %ldms of runtime (%ldms allowed)",
                               lround(time * 1000), lround(threshold * 1000));
  std::unique_ptr<SourceLocation> location;
  if (m_eventListener) {
    location = getFunctionLocation(m_context, m_eventListener.get());
  } else {
    location = m_scheduledAction->handlerLocation();
  }
  m_performanceMonitor->reportGenericViolation(m_violation, text, time,
                                               location.get());
}

// static
void PerformanceMonitor::willExecuteScript(ExecutionContext* context) {
  PerformanceMonitor* performanceMonitor =
      PerformanceMonitor::instrumentingMonitor(context);
  if (performanceMonitor)
    performanceMonitor->innerWillExecuteScript(context);
}

// static
void PerformanceMonitor::didExecuteScript(ExecutionContext* context) {
  PerformanceMonitor* performanceMonitor =
      PerformanceMonitor::instrumentingMonitor(context);
  if (performanceMonitor)
    performanceMonitor->didExecuteScript();
}

// static
void PerformanceMonitor::willUpdateLayout(Document* document) {
  PerformanceMonitor* performanceMonitor =
      PerformanceMonitor::instrumentingMonitor(document);
  if (performanceMonitor)
    performanceMonitor->willUpdateLayout();
}

// static
void PerformanceMonitor::didUpdateLayout(Document* document) {
  PerformanceMonitor* performanceMonitor =
      PerformanceMonitor::instrumentingMonitor(document);
  if (performanceMonitor)
    performanceMonitor->didUpdateLayout();
}

// static
void PerformanceMonitor::willRecalculateStyle(Document* document) {
  PerformanceMonitor* performanceMonitor =
      PerformanceMonitor::instrumentingMonitor(document);
  if (performanceMonitor)
    performanceMonitor->willRecalculateStyle();
}

// static
void PerformanceMonitor::didRecalculateStyle(Document* document) {
  PerformanceMonitor* performanceMonitor =
      PerformanceMonitor::instrumentingMonitor(document);
  if (performanceMonitor)
    performanceMonitor->didRecalculateStyle();
}

// static
void PerformanceMonitor::documentWriteFetchScript(Document* document) {
  PerformanceMonitor* performanceMonitor =
      PerformanceMonitor::instrumentingMonitor(document);
  if (!performanceMonitor)
    return;
  String text = "Parser was blocked due to document.write(<script>)";
  performanceMonitor->reportGenericViolation(
      kBlockedParser, text, 0, SourceLocation::capture(document).get());
}

// static
double PerformanceMonitor::threshold(ExecutionContext* context,
                                     Violation violation) {
  PerformanceMonitor* monitor =
      PerformanceMonitor::instrumentingMonitor(context);
  return monitor ? monitor->m_thresholds[violation] : 0;
}

// static
void PerformanceMonitor::reportGenericViolation(ExecutionContext* context,
                                                Violation violation,
                                                const String& text,
                                                double time,
                                                SourceLocation* location) {
  PerformanceMonitor* monitor =
      PerformanceMonitor::instrumentingMonitor(context);
  if (!monitor)
    return;
  monitor->reportGenericViolation(violation, text, time, location);
}

// static
PerformanceMonitor* PerformanceMonitor::monitor(
    const ExecutionContext* context) {
  if (!context || !context->isDocument())
    return nullptr;
  LocalFrame* frame = toDocument(context)->frame();
  if (!frame)
    return nullptr;
  return frame->performanceMonitor();
}

// static
PerformanceMonitor* PerformanceMonitor::instrumentingMonitor(
    const ExecutionContext* context) {
  PerformanceMonitor* monitor = PerformanceMonitor::monitor(context);
  return monitor && monitor->m_enabled ? monitor : nullptr;
}

PerformanceMonitor::PerformanceMonitor(LocalFrame* localRoot)
    : m_localRoot(localRoot) {
  std::fill(std::begin(m_thresholds), std::end(m_thresholds), 0);
}

PerformanceMonitor::~PerformanceMonitor() {
  m_subscriptions.clear();
  updateInstrumentation();
}

void PerformanceMonitor::subscribe(Violation violation,
                                   double threshold,
                                   Client* client) {
  DCHECK(violation < kAfterLast);
  ClientThresholds* clientThresholds = m_subscriptions.get(violation);
  if (!clientThresholds) {
    clientThresholds = new ClientThresholds();
    m_subscriptions.set(violation, clientThresholds);
  }
  clientThresholds->set(client, threshold);
  updateInstrumentation();
}

void PerformanceMonitor::unsubscribeAll(Client* client) {
  for (const auto& it : m_subscriptions)
    it.value->remove(client);
  updateInstrumentation();
}

void PerformanceMonitor::updateInstrumentation() {
  bool longTaskObserverEnabled = !!m_thresholds[kLongTask];
  std::fill(std::begin(m_thresholds), std::end(m_thresholds), 0);

  for (const auto& it : m_subscriptions) {
    Violation violation = static_cast<Violation>(it.key);
    ClientThresholds* clientThresholds = it.value;
    for (const auto& clientThreshold : *clientThresholds) {
      if (!m_thresholds[violation] ||
          m_thresholds[violation] > clientThreshold.value)
        m_thresholds[violation] = clientThreshold.value;
    }
  }

  if (!m_thresholds[kLongTask] != !longTaskObserverEnabled) {
    if (m_thresholds[kLongTask]) {
      Platform::current()->currentThread()->addTaskTimeObserver(this);
      Platform::current()->currentThread()->addTaskObserver(this);
    } else {
      Platform::current()->currentThread()->removeTaskTimeObserver(this);
      Platform::current()->currentThread()->removeTaskObserver(this);
    }
  }

  m_enabled = std::count(std::begin(m_thresholds), std::end(m_thresholds), 0) <
              static_cast<int>(kAfterLast);
}

void PerformanceMonitor::innerWillExecuteScript(ExecutionContext* context) {
  m_isExecutingScript = true;
  // Heuristic for minimal frame context attribution: note the frame context
  // for each script execution. When a long task is encountered,
  // if there is only one frame context involved, then report it.
  // Otherwise don't report frame context.
  // NOTE: This heuristic is imperfect and will be improved in V2 API.
  // In V2, timing of script execution along with style & layout updates will be
  // accounted for detailed and more accurate attribution.
  if (context->isDocument() && toDocument(context)->frame())
    m_frameContexts.add(toDocument(context)->frame());
}

void PerformanceMonitor::didExecuteScript() {
  m_isExecutingScript = false;
}

void PerformanceMonitor::willUpdateLayout() {
  if (m_isExecutingScript)
    m_layoutStartTime = WTF::monotonicallyIncreasingTime();
}

void PerformanceMonitor::didUpdateLayout() {
  if (m_isExecutingScript) {
    m_perTaskStyleAndLayoutTime +=
        WTF::monotonicallyIncreasingTime() - m_layoutStartTime;
  }
}

void PerformanceMonitor::willRecalculateStyle() {
  if (m_isExecutingScript)
    m_styleStartTime = WTF::monotonicallyIncreasingTime();
}

void PerformanceMonitor::didRecalculateStyle() {
  if (m_isExecutingScript) {
    m_perTaskStyleAndLayoutTime +=
        WTF::monotonicallyIncreasingTime() - m_styleStartTime;
  }
}

void PerformanceMonitor::willProcessTask() {
  m_perTaskStyleAndLayoutTime = 0;
  // Reset m_frameContexts. We don't clear this in didProcessTask
  // as it is needed in ReportTaskTime which occurs after didProcessTask.
  m_frameContexts.clear();
}

void PerformanceMonitor::didProcessTask() {
  double layoutThreshold = m_thresholds[kLongLayout];
  if (!layoutThreshold || m_perTaskStyleAndLayoutTime < layoutThreshold)
    return;
  ClientThresholds* clientThresholds = m_subscriptions.get(kLongLayout);
  DCHECK(clientThresholds);
  for (const auto& it : *clientThresholds) {
    if (it.value < m_perTaskStyleAndLayoutTime)
      it.key->reportLongLayout(m_perTaskStyleAndLayoutTime);
  }
}

void PerformanceMonitor::reportGenericViolation(Violation violation,
                                                const String& text,
                                                double time,
                                                SourceLocation* location) {
  ClientThresholds* clientThresholds = m_subscriptions.get(violation);
  if (!clientThresholds)
    return;
  for (const auto& it : *clientThresholds) {
    if (it.value < time)
      it.key->reportGenericViolation(violation, text, time, location);
  }
}

void PerformanceMonitor::ReportTaskTime(scheduler::TaskQueue*,
                                        double startTime,
                                        double endTime) {
  double taskTime = endTime - startTime;
  if (!m_thresholds[kLongTask] || taskTime < m_thresholds[kLongTask])
    return;

  ClientThresholds* clientThresholds = m_subscriptions.get(kLongTask);
  for (const auto& it : *clientThresholds) {
    if (it.value < taskTime)
      it.key->reportLongTask(startTime, endTime, m_frameContexts);
  }
}

DEFINE_TRACE(PerformanceMonitor) {
  visitor->trace(m_localRoot);
  visitor->trace(m_frameContexts);
  visitor->trace(m_subscriptions);
}

}  // namespace blink
