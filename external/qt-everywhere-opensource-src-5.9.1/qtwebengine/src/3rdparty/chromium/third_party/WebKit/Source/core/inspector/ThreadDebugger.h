// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadDebugger_h
#define ThreadDebugger_h

#include "core/CoreExport.h"
#include "core/inspector/ConsoleTypes.h"
#include "platform/Timer.h"
#include "platform/UserGestureIndicator.h"
#include "wtf/Forward.h"
#include "wtf/Vector.h"
#include <memory>
#include <v8-inspector.h>
#include <v8-profiler.h>
#include <v8.h>

namespace blink {

class ConsoleMessage;
class ExecutionContext;
class SourceLocation;

// TODO(dgozman): rename this to ThreadInspector (and subclasses).
class CORE_EXPORT ThreadDebugger : public v8_inspector::V8InspectorClient {
  WTF_MAKE_NONCOPYABLE(ThreadDebugger);

 public:
  explicit ThreadDebugger(v8::Isolate*);
  ~ThreadDebugger() override;

  static ThreadDebugger* from(v8::Isolate*);
  virtual bool isWorker() = 0;
  v8_inspector::V8Inspector* v8Inspector() const { return m_v8Inspector.get(); }

  static void willExecuteScript(v8::Isolate*, int scriptId);
  static void didExecuteScript(v8::Isolate*);
  static void idleStarted(v8::Isolate*);
  static void idleFinished(v8::Isolate*);

  void asyncTaskScheduled(const String& taskName, void* task, bool recurring);
  void asyncTaskCanceled(void* task);
  void allAsyncTasksCanceled();
  void asyncTaskStarted(void* task);
  void asyncTaskFinished(void* task);
  unsigned promiseRejected(v8::Local<v8::Context>,
                           const String& errorMessage,
                           v8::Local<v8::Value> exception,
                           std::unique_ptr<SourceLocation>);
  void promiseRejectionRevoked(v8::Local<v8::Context>,
                               unsigned promiseRejectionId);

 protected:
  virtual int contextGroupId(ExecutionContext*) = 0;
  virtual void reportConsoleMessage(ExecutionContext*,
                                    MessageSource,
                                    MessageLevel,
                                    const String& message,
                                    SourceLocation*) = 0;
  void installAdditionalCommandLineAPI(v8::Local<v8::Context>,
                                       v8::Local<v8::Object>) override;
  void createFunctionProperty(v8::Local<v8::Context>,
                              v8::Local<v8::Object>,
                              const char* name,
                              v8::FunctionCallback,
                              const char* description);
  static v8::Maybe<bool> createDataPropertyInArray(v8::Local<v8::Context>,
                                                   v8::Local<v8::Array>,
                                                   int index,
                                                   v8::Local<v8::Value>);
  static MessageLevel consoleAPITypeToMessageLevel(
      v8_inspector::V8ConsoleAPIType);

  v8::Isolate* m_isolate;

 private:
  // V8InspectorClient implementation.
  void beginUserGesture() override;
  void endUserGesture() override;
  std::unique_ptr<v8_inspector::StringBuffer> valueSubtype(
      v8::Local<v8::Value>) override;
  bool formatAccessorsAsProperties(v8::Local<v8::Value>) override;
  double currentTimeMS() override;
  bool isInspectableHeapObject(v8::Local<v8::Object>) override;
  void consoleTime(const v8_inspector::StringView& title) override;
  void consoleTimeEnd(const v8_inspector::StringView& title) override;
  void consoleTimeStamp(const v8_inspector::StringView& title) override;
  void startRepeatingTimer(double,
                           v8_inspector::V8InspectorClient::TimerCallback,
                           void* data) override;
  void cancelTimer(void* data) override;

  void onTimer(TimerBase*);

  static void setMonitorEventsCallback(
      const v8::FunctionCallbackInfo<v8::Value>&,
      bool enabled);
  static void monitorEventsCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void unmonitorEventsCallback(
      const v8::FunctionCallbackInfo<v8::Value>&);

  static void getEventListenersCallback(
      const v8::FunctionCallbackInfo<v8::Value>&);

  std::unique_ptr<v8_inspector::V8Inspector> m_v8Inspector;
  std::unique_ptr<v8::TracingCpuProfiler> m_v8TracingCpuProfiler;
  Vector<std::unique_ptr<Timer<ThreadDebugger>>> m_timers;
  Vector<v8_inspector::V8InspectorClient::TimerCallback> m_timerCallbacks;
  Vector<void*> m_timerData;
  std::unique_ptr<UserGestureIndicator> m_userGestureIndicator;
};

}  // namespace blink

#endif  // ThreadDebugger_h
