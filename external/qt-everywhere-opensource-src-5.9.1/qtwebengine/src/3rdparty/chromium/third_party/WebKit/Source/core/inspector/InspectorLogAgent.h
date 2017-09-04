// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorLogAgent_h
#define InspectorLogAgent_h

#include "core/CoreExport.h"
#include "core/frame/PerformanceMonitor.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Log.h"

namespace blink {

class ConsoleMessage;
class ConsoleMessageStorage;

class CORE_EXPORT InspectorLogAgent
    : public InspectorBaseAgent<protocol::Log::Metainfo>,
      public PerformanceMonitor::Client {
  USING_GARBAGE_COLLECTED_MIXIN(InspectorLogAgent);
  WTF_MAKE_NONCOPYABLE(InspectorLogAgent);

 public:
  InspectorLogAgent(ConsoleMessageStorage*, PerformanceMonitor*);
  ~InspectorLogAgent() override;
  DECLARE_VIRTUAL_TRACE();

  void restore() override;

  // Called from InspectorInstrumentation.
  void consoleMessageAdded(ConsoleMessage*);

  // Protocol methods.
  Response enable() override;
  Response disable() override;
  Response clear() override;
  Response startViolationsReport(
      std::unique_ptr<protocol::Array<protocol::Log::ViolationSetting>>)
      override;
  Response stopViolationsReport() override;

 private:
  // PerformanceMonitor::Client implementation.
  void reportLongTask(double startTime,
                      double endTime,
                      const HeapHashSet<Member<Frame>>& contextFrames) override;
  void reportLongLayout(double duration) override;
  void reportGenericViolation(PerformanceMonitor::Violation,
                              const String& text,
                              double time,
                              SourceLocation*) override;

  bool m_enabled;
  Member<ConsoleMessageStorage> m_storage;
  Member<PerformanceMonitor> m_performanceMonitor;
};

}  // namespace blink

#endif  // !defined(InspectorLogAgent_h)
