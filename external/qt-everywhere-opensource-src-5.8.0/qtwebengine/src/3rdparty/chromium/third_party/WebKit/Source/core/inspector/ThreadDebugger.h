// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadDebugger_h
#define ThreadDebugger_h

#include "core/CoreExport.h"
#include "platform/Timer.h"
#include "platform/UserGestureIndicator.h"
#include "platform/v8_inspector/public/V8Debugger.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"
#include "wtf/Forward.h"
#include "wtf/Vector.h"
#include <memory>
#include <v8.h>

namespace blink {

class ConsoleMessage;
class WorkerThread;

class CORE_EXPORT ThreadDebugger : public V8DebuggerClient {
    WTF_MAKE_NONCOPYABLE(ThreadDebugger);
public:
    explicit ThreadDebugger(v8::Isolate*);
    ~ThreadDebugger() override;
    static ThreadDebugger* from(v8::Isolate*);

    static void willExecuteScript(v8::Isolate*, int scriptId);
    static void didExecuteScript(v8::Isolate*);
    static void idleStarted(v8::Isolate*);
    static void idleFinished(v8::Isolate*);

    void asyncTaskScheduled(const String& taskName, void* task, bool recurring);
    void asyncTaskCanceled(void* task);
    void allAsyncTasksCanceled();
    void asyncTaskStarted(void* task);
    void asyncTaskFinished(void* task);

    // V8DebuggerClient implementation.
    void beginUserGesture() override;
    void endUserGesture() override;
    String16 valueSubtype(v8::Local<v8::Value>) override;
    bool formatAccessorsAsProperties(v8::Local<v8::Value>) override;
    bool isExecutionAllowed() override;
    double currentTimeMS() override;
    bool isInspectableHeapObject(v8::Local<v8::Object>) override;
    void enableAsyncInstrumentation() override;
    void disableAsyncInstrumentation() override;
    void installAdditionalCommandLineAPI(v8::Local<v8::Context>, v8::Local<v8::Object>) override;
    void consoleTime(const String16& title) override;
    void consoleTimeEnd(const String16& title) override;
    void consoleTimeStamp(const String16& title) override;
    void startRepeatingTimer(double, V8DebuggerClient::TimerCallback, void* data) override;
    void cancelTimer(void* data) override;

    V8Debugger* debugger() const { return m_debugger.get(); }
    virtual bool isWorker() { return true; }

protected:
    void createFunctionProperty(v8::Local<v8::Context>, v8::Local<v8::Object>, const char* name, v8::FunctionCallback, const char* description);
    static v8::Maybe<bool> createDataPropertyInArray(v8::Local<v8::Context>, v8::Local<v8::Array>, int index, v8::Local<v8::Value>);
    void onTimer(Timer<ThreadDebugger>*);

    v8::Isolate* m_isolate;
    std::unique_ptr<V8Debugger> m_debugger;

private:
    v8::Local<v8::Function> eventLogFunction();

    static void setMonitorEventsCallback(const v8::FunctionCallbackInfo<v8::Value>&, bool enabled);
    static void monitorEventsCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void unmonitorEventsCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void logCallback(const v8::FunctionCallbackInfo<v8::Value>&);

    static void getEventListenersCallback(const v8::FunctionCallbackInfo<v8::Value>&);

    bool m_asyncInstrumentationEnabled;
    Vector<std::unique_ptr<Timer<ThreadDebugger>>> m_timers;
    Vector<V8DebuggerClient::TimerCallback> m_timerCallbacks;
    Vector<void*> m_timerData;
    std::unique_ptr<UserGestureIndicator> m_userGestureIndicator;
    v8::Global<v8::Function> m_eventLogFunction;
};

} // namespace blink

#endif // ThreadDebugger_h
