// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformanceMonitor_h
#define PerformanceMonitor_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebThread.h"
#include "public/platform/scheduler/base/task_time_observer.h"

namespace blink {

class DOMWindow;
class Document;
class EventListener;
class ExecutionContext;
class Frame;
class LocalFrame;
class Performance;
class ScheduledAction;
class SourceLocation;

// Performance monitor for Web Performance APIs and logging.
// The monitor is maintained per local root.
// Long task notifications are delivered to observing Performance* instances
// (in the local frame tree) in m_webPerformanceObservers.
class CORE_EXPORT PerformanceMonitor final
    : public GarbageCollectedFinalized<PerformanceMonitor>,
      public WebThread::TaskObserver,
      public scheduler::TaskTimeObserver {
  WTF_MAKE_NONCOPYABLE(PerformanceMonitor);

 public:
  enum Violation : size_t {
    kLongTask,
    kLongLayout,
    kBlockedEvent,
    kBlockedParser,
    kHandler,
    kRecurringHandler,
    kAfterLast
  };

  class CORE_EXPORT HandlerCall {
    STACK_ALLOCATED();

   public:
    HandlerCall(ExecutionContext*, ScheduledAction*);
    HandlerCall(ExecutionContext*, EventListener*);
    ~HandlerCall();

   private:
    void start();

    Member<ExecutionContext> m_context;
    Member<PerformanceMonitor> m_performanceMonitor;
    Member<ScheduledAction> m_scheduledAction;
    Member<EventListener> m_eventListener;
    Violation m_violation;
    double m_startTime = 0;
  };

  class CORE_EXPORT Client : public GarbageCollectedMixin {
   public:
    virtual void reportLongTask(
        double startTime,
        double endTime,
        const HeapHashSet<Member<Frame>>& contextFrames){};
    virtual void reportLongLayout(double duration){};
    virtual void reportGenericViolation(Violation,
                                        const String& text,
                                        double time,
                                        SourceLocation*){};
    DEFINE_INLINE_VIRTUAL_TRACE() {}
  };

  // Instrumenting methods.
  static void willExecuteScript(ExecutionContext*);
  static void didExecuteScript(ExecutionContext*);
  static void willUpdateLayout(Document*);
  static void didUpdateLayout(Document*);
  static void willRecalculateStyle(Document*);
  static void didRecalculateStyle(Document*);
  static void documentWriteFetchScript(Document*);
  static void reportGenericViolation(ExecutionContext*,
                                     Violation,
                                     const String& text,
                                     double time,
                                     SourceLocation*);
  static double threshold(ExecutionContext*, Violation);

  // Direct API for core.
  void subscribe(Violation, double threshold, Client*);
  void unsubscribeAll(Client*);

  explicit PerformanceMonitor(LocalFrame*);
  ~PerformanceMonitor();

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class PerformanceMonitorTest;
  friend class PerformanceTest;

  static PerformanceMonitor* monitor(const ExecutionContext*);
  static PerformanceMonitor* instrumentingMonitor(const ExecutionContext*);

  void updateInstrumentation();

  void innerWillExecuteScript(ExecutionContext*);
  void didExecuteScript();
  void willUpdateLayout();
  void didUpdateLayout();
  void willRecalculateStyle();
  void didRecalculateStyle();
  void reportGenericViolation(Violation,
                              const String& text,
                              double time,
                              SourceLocation*);

  // WebThread::TaskObserver implementation.
  void willProcessTask() override;
  void didProcessTask() override;

  // scheduler::TaskTimeObserver implementation
  void ReportTaskTime(scheduler::TaskQueue*,
                      double startTime,
                      double endTime) override;

  std::pair<String, DOMWindow*> sanitizedAttribution(
      const HeapHashSet<Member<Frame>>& frameContexts,
      Frame* observerFrame);

  bool m_enabled = false;
  bool m_isExecutingScript = false;
  double m_layoutStartTime = 0;
  double m_styleStartTime = 0;
  double m_perTaskStyleAndLayoutTime = 0;

  double m_thresholds[kAfterLast];

  Member<LocalFrame> m_localRoot;
  HeapHashSet<Member<Frame>> m_frameContexts;
  using ClientThresholds = HeapHashMap<Member<Client>, double>;
  HeapHashMap<Violation,
              Member<ClientThresholds>,
              typename DefaultHash<size_t>::Hash,
              WTF::UnsignedWithZeroKeyHashTraits<size_t>>
      m_subscriptions;
};

}  // namespace blink

#endif  // PerformanceMonitor_h
