// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/ThreadDebugger.h"

#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMException.h"
#include "bindings/core/v8/V8DOMTokenList.h"
#include "bindings/core/v8/V8Event.h"
#include "bindings/core/v8/V8EventListener.h"
#include "bindings/core/v8/V8EventListenerHelper.h"
#include "bindings/core/v8/V8EventListenerInfo.h"
#include "bindings/core/v8/V8HTMLAllCollection.h"
#include "bindings/core/v8/V8HTMLCollection.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/core/v8/V8NodeList.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorDOMDebuggerAgent.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/V8InspectorString.h"
#include "platform/ScriptForbiddenScope.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

ThreadDebugger::ThreadDebugger(v8::Isolate* isolate)
    : m_isolate(isolate),
      m_v8Inspector(v8_inspector::V8Inspector::create(isolate, this)),
      m_v8TracingCpuProfiler(v8::TracingCpuProfiler::Create(isolate)) {}

ThreadDebugger::~ThreadDebugger() {}

// static
ThreadDebugger* ThreadDebugger::from(v8::Isolate* isolate) {
  if (!isolate)
    return nullptr;
  V8PerIsolateData* data = V8PerIsolateData::from(isolate);
  return data ? data->threadDebugger() : nullptr;
}

// static
MessageLevel ThreadDebugger::consoleAPITypeToMessageLevel(
    v8_inspector::V8ConsoleAPIType type) {
  switch (type) {
    case v8_inspector::V8ConsoleAPIType::kDebug:
      return DebugMessageLevel;
    case v8_inspector::V8ConsoleAPIType::kLog:
      return LogMessageLevel;
    case v8_inspector::V8ConsoleAPIType::kInfo:
      return InfoMessageLevel;
    case v8_inspector::V8ConsoleAPIType::kWarning:
      return WarningMessageLevel;
    case v8_inspector::V8ConsoleAPIType::kError:
      return ErrorMessageLevel;
    default:
      return LogMessageLevel;
  }
}

void ThreadDebugger::willExecuteScript(v8::Isolate* isolate, int scriptId) {
  if (ThreadDebugger* debugger = ThreadDebugger::from(isolate))
    debugger->v8Inspector()->willExecuteScript(isolate->GetCurrentContext(),
                                               scriptId);
}

void ThreadDebugger::didExecuteScript(v8::Isolate* isolate) {
  if (ThreadDebugger* debugger = ThreadDebugger::from(isolate))
    debugger->v8Inspector()->didExecuteScript(isolate->GetCurrentContext());
}

void ThreadDebugger::idleStarted(v8::Isolate* isolate) {
  if (ThreadDebugger* debugger = ThreadDebugger::from(isolate))
    debugger->v8Inspector()->idleStarted();
}

void ThreadDebugger::idleFinished(v8::Isolate* isolate) {
  if (ThreadDebugger* debugger = ThreadDebugger::from(isolate))
    debugger->v8Inspector()->idleFinished();
}

void ThreadDebugger::asyncTaskScheduled(const String& operationName,
                                        void* task,
                                        bool recurring) {
  m_v8Inspector->asyncTaskScheduled(toV8InspectorStringView(operationName),
                                    task, recurring);
}

void ThreadDebugger::asyncTaskCanceled(void* task) {
  m_v8Inspector->asyncTaskCanceled(task);
}

void ThreadDebugger::allAsyncTasksCanceled() {
  m_v8Inspector->allAsyncTasksCanceled();
}

void ThreadDebugger::asyncTaskStarted(void* task) {
  m_v8Inspector->asyncTaskStarted(task);
}

void ThreadDebugger::asyncTaskFinished(void* task) {
  m_v8Inspector->asyncTaskFinished(task);
}

unsigned ThreadDebugger::promiseRejected(
    v8::Local<v8::Context> context,
    const String& errorMessage,
    v8::Local<v8::Value> exception,
    std::unique_ptr<SourceLocation> location) {
  const String defaultMessage = "Uncaught (in promise)";
  String message = errorMessage;
  if (message.isEmpty())
    message = defaultMessage;
  else if (message.startsWith("Uncaught "))
    message = message.substring(0, 8) + " (in promise)" + message.substring(8);

  reportConsoleMessage(toExecutionContext(context), JSMessageSource,
                       ErrorMessageLevel, message, location.get());
  String url = location->url();
  return v8Inspector()->exceptionThrown(
      context, toV8InspectorStringView(defaultMessage), exception,
      toV8InspectorStringView(message), toV8InspectorStringView(url),
      location->lineNumber(), location->columnNumber(),
      location->takeStackTrace(), location->scriptId());
}

void ThreadDebugger::promiseRejectionRevoked(v8::Local<v8::Context> context,
                                             unsigned promiseRejectionId) {
  const String message = "Handler added to rejected promise";
  v8Inspector()->exceptionRevoked(context, promiseRejectionId,
                                  toV8InspectorStringView(message));
}

void ThreadDebugger::beginUserGesture() {
  m_userGestureIndicator = wrapUnique(
      new UserGestureIndicator(DocumentUserGestureToken::create(nullptr)));
}

void ThreadDebugger::endUserGesture() {
  m_userGestureIndicator.reset();
}

std::unique_ptr<v8_inspector::StringBuffer> ThreadDebugger::valueSubtype(
    v8::Local<v8::Value> value) {
  static const char kNode[] = "node";
  static const char kArray[] = "array";
  static const char kError[] = "error";
  if (V8Node::hasInstance(value, m_isolate))
    return toV8InspectorStringBuffer(kNode);
  if (V8NodeList::hasInstance(value, m_isolate) ||
      V8DOMTokenList::hasInstance(value, m_isolate) ||
      V8HTMLCollection::hasInstance(value, m_isolate) ||
      V8HTMLAllCollection::hasInstance(value, m_isolate)) {
    return toV8InspectorStringBuffer(kArray);
  }
  if (V8DOMException::hasInstance(value, m_isolate))
    return toV8InspectorStringBuffer(kError);
  return nullptr;
}

bool ThreadDebugger::formatAccessorsAsProperties(v8::Local<v8::Value> value) {
  return V8DOMWrapper::isWrapper(m_isolate, value);
}

double ThreadDebugger::currentTimeMS() {
  return WTF::currentTimeMS();
}

bool ThreadDebugger::isInspectableHeapObject(v8::Local<v8::Object> object) {
  if (object->InternalFieldCount() < v8DefaultWrapperInternalFieldCount)
    return true;
  v8::Local<v8::Value> wrapper =
      object->GetInternalField(v8DOMWrapperObjectIndex);
  // Skip wrapper boilerplates which are like regular wrappers but don't have
  // native object.
  if (!wrapper.IsEmpty() && wrapper->IsUndefined())
    return false;
  return true;
}

static void returnDataCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(info.Data());
}

static v8::Maybe<bool> createDataProperty(v8::Local<v8::Context> context,
                                          v8::Local<v8::Object> object,
                                          v8::Local<v8::Name> key,
                                          v8::Local<v8::Value> value) {
  v8::TryCatch tryCatch(context->GetIsolate());
  v8::Isolate::DisallowJavascriptExecutionScope throwJs(
      context->GetIsolate(),
      v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
  return object->CreateDataProperty(context, key, value);
}

static void createFunctionPropertyWithData(v8::Local<v8::Context> context,
                                           v8::Local<v8::Object> object,
                                           const char* name,
                                           v8::FunctionCallback callback,
                                           v8::Local<v8::Value> data,
                                           const char* description) {
  v8::Local<v8::String> funcName = v8String(context->GetIsolate(), name);
  v8::Local<v8::Function> func;
  if (!v8::Function::New(context, callback, data, 0,
                         v8::ConstructorBehavior::kThrow)
           .ToLocal(&func))
    return;
  func->SetName(funcName);
  v8::Local<v8::String> returnValue =
      v8String(context->GetIsolate(), description);
  v8::Local<v8::Function> toStringFunction;
  if (v8::Function::New(context, returnDataCallback, returnValue, 0,
                        v8::ConstructorBehavior::kThrow)
          .ToLocal(&toStringFunction))
    createDataProperty(context, func,
                       v8String(context->GetIsolate(), "toString"),
                       toStringFunction);
  createDataProperty(context, object, funcName, func);
}

v8::Maybe<bool> ThreadDebugger::createDataPropertyInArray(
    v8::Local<v8::Context> context,
    v8::Local<v8::Array> array,
    int index,
    v8::Local<v8::Value> value) {
  v8::TryCatch tryCatch(context->GetIsolate());
  v8::Isolate::DisallowJavascriptExecutionScope throwJs(
      context->GetIsolate(),
      v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
  return array->CreateDataProperty(context, index, value);
}

void ThreadDebugger::createFunctionProperty(v8::Local<v8::Context> context,
                                            v8::Local<v8::Object> object,
                                            const char* name,
                                            v8::FunctionCallback callback,
                                            const char* description) {
  createFunctionPropertyWithData(context, object, name, callback,
                                 v8::External::New(context->GetIsolate(), this),
                                 description);
}

void ThreadDebugger::installAdditionalCommandLineAPI(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object) {
  createFunctionProperty(
      context, object, "getEventListeners",
      ThreadDebugger::getEventListenersCallback,
      "function getEventListeners(node) { [Command Line API] }");

  v8::Local<v8::Value> functionValue;
  bool success =
      V8ScriptRunner::compileAndRunInternalScript(
          v8String(m_isolate, "(function(e) { console.log(e.type, e); })"),
          m_isolate)
          .ToLocal(&functionValue) &&
      functionValue->IsFunction();
  DCHECK(success);
  createFunctionPropertyWithData(
      context, object, "monitorEvents", ThreadDebugger::monitorEventsCallback,
      functionValue,
      "function monitorEvents(object, [types]) { [Command Line API] }");
  createFunctionPropertyWithData(
      context, object, "unmonitorEvents",
      ThreadDebugger::unmonitorEventsCallback, functionValue,
      "function unmonitorEvents(object, [types]) { [Command Line API] }");
}

static Vector<String> normalizeEventTypes(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  Vector<String> types;
  if (info.Length() > 1 && info[1]->IsString())
    types.append(toCoreString(info[1].As<v8::String>()));
  if (info.Length() > 1 && info[1]->IsArray()) {
    v8::Local<v8::Array> typesArray = v8::Local<v8::Array>::Cast(info[1]);
    for (size_t i = 0; i < typesArray->Length(); ++i) {
      v8::Local<v8::Value> typeValue;
      if (!typesArray->Get(info.GetIsolate()->GetCurrentContext(), i)
               .ToLocal(&typeValue) ||
          !typeValue->IsString())
        continue;
      types.append(toCoreString(v8::Local<v8::String>::Cast(typeValue)));
    }
  }
  if (info.Length() == 1)
    types.appendVector(
        Vector<String>({"mouse",   "key",          "touch",
                        "pointer", "control",      "load",
                        "unload",  "abort",        "error",
                        "select",  "input",        "change",
                        "submit",  "reset",        "focus",
                        "blur",    "resize",       "scroll",
                        "search",  "devicemotion", "deviceorientation"}));

  Vector<String> outputTypes;
  for (size_t i = 0; i < types.size(); ++i) {
    if (types[i] == "mouse")
      outputTypes.appendVector(
          Vector<String>({"auxclick", "click", "dblclick", "mousedown",
                          "mouseeenter", "mouseleave", "mousemove", "mouseout",
                          "mouseover", "mouseup", "mouseleave", "mousewheel"}));
    else if (types[i] == "key")
      outputTypes.appendVector(
          Vector<String>({"keydown", "keyup", "keypress", "textInput"}));
    else if (types[i] == "touch")
      outputTypes.appendVector(Vector<String>(
          {"touchstart", "touchmove", "touchend", "touchcancel"}));
    else if (types[i] == "pointer")
      outputTypes.appendVector(Vector<String>(
          {"pointerover", "pointerout", "pointerenter", "pointerleave",
           "pointerdown", "pointerup", "pointermove", "pointercancel",
           "gotpointercapture", "lostpointercapture"}));
    else if (types[i] == "control")
      outputTypes.appendVector(
          Vector<String>({"resize", "scroll", "zoom", "focus", "blur", "select",
                          "input", "change", "submit", "reset"}));
    else
      outputTypes.append(types[i]);
  }
  return outputTypes;
}

static EventTarget* firstArgumentAsEventTarget(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 1)
    return nullptr;
  if (EventTarget* target =
          V8EventTarget::toImplWithTypeCheck(info.GetIsolate(), info[0]))
    return target;
  return toDOMWindow(info.GetIsolate(), info[0]);
}

void ThreadDebugger::setMonitorEventsCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    bool enabled) {
  EventTarget* eventTarget = firstArgumentAsEventTarget(info);
  if (!eventTarget)
    return;
  Vector<String> types = normalizeEventTypes(info);
  EventListener* eventListener = V8EventListenerHelper::getEventListener(
      ScriptState::current(info.GetIsolate()),
      v8::Local<v8::Function>::Cast(info.Data()), false,
      enabled ? ListenerFindOrCreate : ListenerFindOnly);
  if (!eventListener)
    return;
  for (size_t i = 0; i < types.size(); ++i) {
    if (enabled)
      eventTarget->addEventListener(AtomicString(types[i]), eventListener,
                                    false);
    else
      eventTarget->removeEventListener(AtomicString(types[i]), eventListener,
                                       false);
  }
}

// static
void ThreadDebugger::monitorEventsCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  setMonitorEventsCallback(info, true);
}

// static
void ThreadDebugger::unmonitorEventsCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  setMonitorEventsCallback(info, false);
}

// static
void ThreadDebugger::getEventListenersCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 1)
    return;

  ThreadDebugger* debugger = static_cast<ThreadDebugger*>(
      v8::Local<v8::External>::Cast(info.Data())->Value());
  DCHECK(debugger);
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  int groupId = debugger->contextGroupId(toExecutionContext(context));

  V8EventListenerInfoList listenerInfo;
  // eventListeners call can produce message on ErrorEvent during lazy event
  // listener compilation.
  if (groupId)
    debugger->muteMetrics(groupId);
  InspectorDOMDebuggerAgent::eventListenersInfoForTarget(isolate, info[0],
                                                         listenerInfo);
  if (groupId)
    debugger->unmuteMetrics(groupId);

  v8::Local<v8::Object> result = v8::Object::New(isolate);
  AtomicString currentEventType;
  v8::Local<v8::Array> listeners;
  size_t outputIndex = 0;
  for (auto& info : listenerInfo) {
    if (currentEventType != info.eventType) {
      currentEventType = info.eventType;
      listeners = v8::Array::New(isolate);
      outputIndex = 0;
      createDataProperty(context, result, v8String(isolate, currentEventType),
                         listeners);
    }

    v8::Local<v8::Object> listenerObject = v8::Object::New(isolate);
    createDataProperty(context, listenerObject, v8String(isolate, "listener"),
                       info.handler);
    createDataProperty(context, listenerObject, v8String(isolate, "useCapture"),
                       v8::Boolean::New(isolate, info.useCapture));
    createDataProperty(context, listenerObject, v8String(isolate, "passive"),
                       v8::Boolean::New(isolate, info.passive));
    createDataProperty(context, listenerObject, v8String(isolate, "once"),
                       v8::Boolean::New(isolate, info.once));
    createDataProperty(context, listenerObject, v8String(isolate, "type"),
                       v8String(isolate, currentEventType));
    v8::Local<v8::Function> removeFunction;
    if (info.removeFunction.ToLocal(&removeFunction))
      createDataProperty(context, listenerObject, v8String(isolate, "remove"),
                         removeFunction);
    createDataPropertyInArray(context, listeners, outputIndex++,
                              listenerObject);
  }
  info.GetReturnValue().Set(result);
}

void ThreadDebugger::consoleTime(const v8_inspector::StringView& title) {
  // TODO(dgozman): we can save on a copy here if trace macro would take a
  // pointer with length.
  TRACE_EVENT_COPY_ASYNC_BEGIN0("blink.console",
                                toCoreString(title).utf8().data(), this);
}

void ThreadDebugger::consoleTimeEnd(const v8_inspector::StringView& title) {
  // TODO(dgozman): we can save on a copy here if trace macro would take a
  // pointer with length.
  TRACE_EVENT_COPY_ASYNC_END0("blink.console",
                              toCoreString(title).utf8().data(), this);
}

void ThreadDebugger::consoleTimeStamp(const v8_inspector::StringView& title) {
  v8::Isolate* isolate = m_isolate;
  // TODO(dgozman): we can save on a copy here if TracedValue would take a
  // StringView.
  TRACE_EVENT_INSTANT1(
      "devtools.timeline", "TimeStamp", TRACE_EVENT_SCOPE_THREAD, "data",
      InspectorTimeStampEvent::data(currentExecutionContext(isolate),
                                    toCoreString(title)));
}

void ThreadDebugger::startRepeatingTimer(
    double interval,
    V8InspectorClient::TimerCallback callback,
    void* data) {
  m_timerData.append(data);
  m_timerCallbacks.append(callback);

  std::unique_ptr<Timer<ThreadDebugger>> timer =
      wrapUnique(new Timer<ThreadDebugger>(this, &ThreadDebugger::onTimer));
  Timer<ThreadDebugger>* timerPtr = timer.get();
  m_timers.append(std::move(timer));
  timerPtr->startRepeating(interval, BLINK_FROM_HERE);
}

void ThreadDebugger::cancelTimer(void* data) {
  for (size_t index = 0; index < m_timerData.size(); ++index) {
    if (m_timerData[index] == data) {
      m_timers[index]->stop();
      m_timerCallbacks.remove(index);
      m_timers.remove(index);
      m_timerData.remove(index);
      return;
    }
  }
}

void ThreadDebugger::onTimer(TimerBase* timer) {
  for (size_t index = 0; index < m_timers.size(); ++index) {
    if (m_timers[index].get() == timer) {
      m_timerCallbacks[index](m_timerData[index]);
      return;
    }
  }
}

}  // namespace blink
