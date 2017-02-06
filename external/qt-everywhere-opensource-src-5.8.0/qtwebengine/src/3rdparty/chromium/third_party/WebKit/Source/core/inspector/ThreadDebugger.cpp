// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/ThreadDebugger.h"

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMException.h"
#include "bindings/core/v8/V8DOMTokenList.h"
#include "bindings/core/v8/V8Event.h"
#include "bindings/core/v8/V8EventListener.h"
#include "bindings/core/v8/V8EventListenerInfo.h"
#include "bindings/core/v8/V8EventListenerList.h"
#include "bindings/core/v8/V8HTMLAllCollection.h"
#include "bindings/core/v8/V8HTMLCollection.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/core/v8/V8NodeList.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorDOMDebuggerAgent.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/ScriptForbiddenScope.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

ThreadDebugger::ThreadDebugger(v8::Isolate* isolate)
    : m_isolate(isolate)
    , m_debugger(V8Debugger::create(isolate, this))
    , m_asyncInstrumentationEnabled(false)
{
}

ThreadDebugger::~ThreadDebugger()
{
}

// static
ThreadDebugger* ThreadDebugger::from(v8::Isolate* isolate)
{
    if (!isolate)
        return nullptr;
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    return data ? data->threadDebugger() : nullptr;
}

void ThreadDebugger::willExecuteScript(v8::Isolate* isolate, int scriptId)
{
    if (ThreadDebugger* debugger = ThreadDebugger::from(isolate))
        debugger->debugger()->willExecuteScript(isolate->GetCurrentContext(), scriptId);
}

void ThreadDebugger::didExecuteScript(v8::Isolate* isolate)
{
    if (ThreadDebugger* debugger = ThreadDebugger::from(isolate))
        debugger->debugger()->didExecuteScript(isolate->GetCurrentContext());
}

void ThreadDebugger::idleStarted(v8::Isolate* isolate)
{
    if (ThreadDebugger* debugger = ThreadDebugger::from(isolate))
        debugger->debugger()->idleStarted();
}

void ThreadDebugger::idleFinished(v8::Isolate* isolate)
{
    if (ThreadDebugger* debugger = ThreadDebugger::from(isolate))
        debugger->debugger()->idleFinished();
}

void ThreadDebugger::asyncTaskScheduled(const String& operationName, void* task, bool recurring)
{
    if (m_asyncInstrumentationEnabled)
        m_debugger->asyncTaskScheduled(operationName, task, recurring);
}

void ThreadDebugger::asyncTaskCanceled(void* task)
{
    if (m_asyncInstrumentationEnabled)
        m_debugger->asyncTaskCanceled(task);
}

void ThreadDebugger::allAsyncTasksCanceled()
{
    if (m_asyncInstrumentationEnabled)
        m_debugger->allAsyncTasksCanceled();
}

void ThreadDebugger::asyncTaskStarted(void* task)
{
    if (m_asyncInstrumentationEnabled)
        m_debugger->asyncTaskStarted(task);
}

void ThreadDebugger::asyncTaskFinished(void* task)
{
    if (m_asyncInstrumentationEnabled)
        m_debugger->asyncTaskFinished(task);
}

void ThreadDebugger::beginUserGesture()
{
    m_userGestureIndicator = wrapUnique(new UserGestureIndicator(DefinitelyProcessingNewUserGesture));
}

void ThreadDebugger::endUserGesture()
{
    m_userGestureIndicator.reset();
}

String16 ThreadDebugger::valueSubtype(v8::Local<v8::Value> value)
{
    if (V8Node::hasInstance(value, m_isolate))
        return "node";
    if (V8NodeList::hasInstance(value, m_isolate)
        || V8DOMTokenList::hasInstance(value, m_isolate)
        || V8HTMLCollection::hasInstance(value, m_isolate)
        || V8HTMLAllCollection::hasInstance(value, m_isolate)) {
        return "array";
    }
    if (V8DOMException::hasInstance(value, m_isolate))
        return "error";
    return String();
}

bool ThreadDebugger::formatAccessorsAsProperties(v8::Local<v8::Value> value)
{
    return V8DOMWrapper::isWrapper(m_isolate, value);
}

bool ThreadDebugger::isExecutionAllowed()
{
    return !ScriptForbiddenScope::isScriptForbidden();
}

double ThreadDebugger::currentTimeMS()
{
    return WTF::currentTimeMS();
}

bool ThreadDebugger::isInspectableHeapObject(v8::Local<v8::Object> object)
{
    if (object->InternalFieldCount() < v8DefaultWrapperInternalFieldCount)
        return true;
    v8::Local<v8::Value> wrapper = object->GetInternalField(v8DOMWrapperObjectIndex);
    // Skip wrapper boilerplates which are like regular wrappers but don't have
    // native object.
    if (!wrapper.IsEmpty() && wrapper->IsUndefined())
        return false;
    return true;
}

void ThreadDebugger::enableAsyncInstrumentation()
{
    DCHECK(!m_asyncInstrumentationEnabled);
    m_asyncInstrumentationEnabled = true;
}

void ThreadDebugger::disableAsyncInstrumentation()
{
    DCHECK(m_asyncInstrumentationEnabled);
    m_asyncInstrumentationEnabled = false;
}

static void returnDataCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    info.GetReturnValue().Set(info.Data());
}

static v8::Maybe<bool> createDataProperty(v8::Local<v8::Context> context, v8::Local<v8::Object> object, v8::Local<v8::Name> key, v8::Local<v8::Value> value)
{
    v8::TryCatch tryCatch(context->GetIsolate());
    v8::Isolate::DisallowJavascriptExecutionScope throwJs(context->GetIsolate(), v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
    return object->CreateDataProperty(context, key, value);
}

static void createFunctionPropertyWithData(v8::Local<v8::Context> context, v8::Local<v8::Object> object, const char* name, v8::FunctionCallback callback, v8::Local<v8::Value> data, const char* description)
{
    v8::Local<v8::String> funcName = v8String(context->GetIsolate(), name);
    v8::Local<v8::Function> func;
    if (!v8::Function::New(context, callback, data, 0, v8::ConstructorBehavior::kThrow).ToLocal(&func))
        return;
    func->SetName(funcName);
    v8::Local<v8::String> returnValue = v8String(context->GetIsolate(), description);
    v8::Local<v8::Function> toStringFunction;
    if (v8::Function::New(context, returnDataCallback, returnValue, 0, v8::ConstructorBehavior::kThrow).ToLocal(&toStringFunction))
        createDataProperty(context, func, v8String(context->GetIsolate(), "toString"), toStringFunction);
    createDataProperty(context, object, funcName, func);
}

void ThreadDebugger::createFunctionProperty(v8::Local<v8::Context> context, v8::Local<v8::Object> object, const char* name, v8::FunctionCallback callback, const char* description)
{
    createFunctionPropertyWithData(context, object, name, callback, v8::External::New(context->GetIsolate(), this), description);
}

v8::Maybe<bool> ThreadDebugger::createDataPropertyInArray(v8::Local<v8::Context> context, v8::Local<v8::Array> array, int index, v8::Local<v8::Value> value)
{
    v8::TryCatch tryCatch(context->GetIsolate());
    v8::Isolate::DisallowJavascriptExecutionScope throwJs(context->GetIsolate(), v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
    return array->CreateDataProperty(context, index, value);
}

void ThreadDebugger::installAdditionalCommandLineAPI(v8::Local<v8::Context> context, v8::Local<v8::Object> object)
{
    createFunctionProperty(context, object, "monitorEvents", ThreadDebugger::monitorEventsCallback, "function monitorEvents(object, [types]) { [Command Line API] }");
    createFunctionProperty(context, object, "unmonitorEvents", ThreadDebugger::unmonitorEventsCallback, "function unmonitorEvents(object, [types]) { [Command Line API] }");
    createFunctionProperty(context, object, "getEventListeners", ThreadDebugger::getEventListenersCallback, "function getEventListeners(node) { [Command Line API] }");
}

static Vector<String> normalizeEventTypes(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    Vector<String> types;
    if (info.Length() > 1 && info[1]->IsString())
        types.append(toCoreString(info[1].As<v8::String>()));
    if (info.Length() > 1 && info[1]->IsArray()) {
        v8::Local<v8::Array> typesArray = v8::Local<v8::Array>::Cast(info[1]);
        for (size_t i = 0; i < typesArray->Length(); ++i) {
            v8::Local<v8::Value> typeValue;
            if (!typesArray->Get(info.GetIsolate()->GetCurrentContext(), i).ToLocal(&typeValue) || !typeValue->IsString())
                continue;
            types.append(toCoreString(v8::Local<v8::String>::Cast(typeValue)));
        }
    }
    if (info.Length() == 1)
        types.appendVector(Vector<String>({ "mouse", "key", "touch", "pointer", "control", "load", "unload", "abort", "error", "select", "input", "change", "submit", "reset", "focus", "blur", "resize", "scroll", "search", "devicemotion", "deviceorientation" }));

    Vector<String> outputTypes;
    for (size_t i = 0; i < types.size(); ++i) {
        if (types[i] == "mouse")
            outputTypes.appendVector(Vector<String>({ "click", "dblclick", "mousedown", "mouseeenter", "mouseleave", "mousemove", "mouseout", "mouseover", "mouseup", "mouseleave", "mousewheel" }));
        else if (types[i] == "key")
            outputTypes.appendVector(Vector<String>({ "keydown", "keyup", "keypress", "textInput" }));
        else if (types[i] == "touch")
            outputTypes.appendVector(Vector<String>({ "touchstart", "touchmove", "touchend", "touchcancel" }));
        else if (types[i] == "pointer")
            outputTypes.appendVector(Vector<String>({ "pointerover", "pointerout", "pointerenter", "pointerleave", "pointerdown", "pointerup", "pointermove", "pointercancel", "gotpointercapture", "lostpointercapture" }));
        else if (types[i] == "control")
            outputTypes.appendVector(Vector<String>({ "resize", "scroll", "zoom", "focus", "blur", "select", "input", "change", "submit", "reset" }));
        else
            outputTypes.append(types[i]);
    }
    return outputTypes;
}

void ThreadDebugger::logCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;
    ThreadDebugger* debugger = static_cast<ThreadDebugger*>(v8::Local<v8::External>::Cast(info.Data())->Value());
    DCHECK(debugger);
    Event* event = V8Event::toImplWithTypeCheck(info.GetIsolate(), info[0]);
    if (!event)
        return;
    debugger->debugger()->logToConsole(info.GetIsolate()->GetCurrentContext(), event->type(), v8String(info.GetIsolate(), event->type()), info[0]);
}

v8::Local<v8::Function> ThreadDebugger::eventLogFunction()
{
    if (m_eventLogFunction.IsEmpty())
        m_eventLogFunction.Reset(m_isolate, v8::Function::New(m_isolate->GetCurrentContext(), logCallback, v8::External::New(m_isolate, this), 0, v8::ConstructorBehavior::kThrow).ToLocalChecked());
    return m_eventLogFunction.Get(m_isolate);
}

static EventTarget* firstArgumentAsEventTarget(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return nullptr;
    if (EventTarget* target = V8EventTarget::toImplWithTypeCheck(info.GetIsolate(), info[0]))
        return target;
    return toDOMWindow(info.GetIsolate(), info[0]);
}

void ThreadDebugger::setMonitorEventsCallback(const v8::FunctionCallbackInfo<v8::Value>& info, bool enabled)
{
    EventTarget* eventTarget = firstArgumentAsEventTarget(info);
    if (!eventTarget)
        return;
    Vector<String> types = normalizeEventTypes(info);
    ThreadDebugger* debugger = static_cast<ThreadDebugger*>(v8::Local<v8::External>::Cast(info.Data())->Value());
    DCHECK(debugger);
    EventListener* eventListener = V8EventListenerList::getEventListener(ScriptState::current(info.GetIsolate()), debugger->eventLogFunction(), false, enabled ? ListenerFindOrCreate : ListenerFindOnly);
    if (!eventListener)
        return;
    for (size_t i = 0; i < types.size(); ++i) {
        if (enabled)
            eventTarget->addEventListener(AtomicString(types[i]), eventListener, false);
        else
            eventTarget->removeEventListener(AtomicString(types[i]), eventListener, false);
    }
}

// static
void ThreadDebugger::monitorEventsCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    setMonitorEventsCallback(info, true);
}

// static
void ThreadDebugger::unmonitorEventsCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    setMonitorEventsCallback(info, false);
}

// static
void ThreadDebugger::getEventListenersCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;

    ThreadDebugger* debugger = static_cast<ThreadDebugger*>(v8::Local<v8::External>::Cast(info.Data())->Value());
    DCHECK(debugger);
    v8::Isolate* isolate = info.GetIsolate();

    V8EventListenerInfoList listenerInfo;
    // eventListeners call can produce message on ErrorEvent during lazy event listener compilation.
    debugger->muteWarningsAndDeprecations();
    debugger->debugger()->muteConsole();
    InspectorDOMDebuggerAgent::eventListenersInfoForTarget(isolate, info[0], listenerInfo);
    debugger->debugger()->unmuteConsole();
    debugger->unmuteWarningsAndDeprecations();

    v8::Local<v8::Object> result = v8::Object::New(isolate);
    AtomicString currentEventType;
    v8::Local<v8::Array> listeners;
    size_t outputIndex = 0;
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    for (auto& info : listenerInfo) {
        if (currentEventType != info.eventType) {
            currentEventType = info.eventType;
            listeners = v8::Array::New(isolate);
            outputIndex = 0;
            createDataProperty(context, result, v8String(isolate, currentEventType), listeners);
        }

        v8::Local<v8::Object> listenerObject = v8::Object::New(isolate);
        createDataProperty(context, listenerObject, v8String(isolate, "listener"), info.handler);
        createDataProperty(context, listenerObject, v8String(isolate, "useCapture"), v8::Boolean::New(isolate, info.useCapture));
        createDataProperty(context, listenerObject, v8String(isolate, "passive"), v8::Boolean::New(isolate, info.passive));
        createDataProperty(context, listenerObject, v8String(isolate, "type"), v8String(isolate, currentEventType));
        v8::Local<v8::Function> removeFunction;
        if (info.removeFunction.ToLocal(&removeFunction))
            createDataProperty(context, listenerObject, v8String(isolate, "remove"), removeFunction);
        createDataPropertyInArray(context, listeners, outputIndex++, listenerObject);
    }
    info.GetReturnValue().Set(result);
}

void ThreadDebugger::consoleTime(const String16& title)
{
    TRACE_EVENT_COPY_ASYNC_BEGIN0("blink.console", String(title).utf8().data(), this);
}

void ThreadDebugger::consoleTimeEnd(const String16& title)
{
    TRACE_EVENT_COPY_ASYNC_END0("blink.console", String(title).utf8().data(), this);
}

void ThreadDebugger::consoleTimeStamp(const String16& title)
{
    v8::Isolate* isolate = m_isolate;
    TRACE_EVENT_INSTANT1("devtools.timeline", "TimeStamp", TRACE_EVENT_SCOPE_THREAD, "data", InspectorTimeStampEvent::data(currentExecutionContext(isolate), title));
}

void ThreadDebugger::startRepeatingTimer(double interval, V8DebuggerClient::TimerCallback callback, void* data)
{
    m_timerData.append(data);
    m_timerCallbacks.append(callback);

    std::unique_ptr<Timer<ThreadDebugger>> timer = wrapUnique(new Timer<ThreadDebugger>(this, &ThreadDebugger::onTimer));
    Timer<ThreadDebugger>* timerPtr = timer.get();
    m_timers.append(std::move(timer));
    timerPtr->startRepeating(interval, BLINK_FROM_HERE);
}

void ThreadDebugger::cancelTimer(void* data)
{
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

void ThreadDebugger::onTimer(Timer<ThreadDebugger>* timer)
{
    for (size_t index = 0; index < m_timers.size(); ++index) {
        if (m_timers[index].get() == timer) {
            m_timerCallbacks[index](m_timerData[index]);
            return;
        }
    }
}

} // namespace blink
