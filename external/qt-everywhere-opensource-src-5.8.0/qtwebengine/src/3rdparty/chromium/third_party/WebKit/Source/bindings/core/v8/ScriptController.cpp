/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/ScriptController.h"

#include "bindings/core/v8/BindingSecurity.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8Event.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8HTMLElement.h"
#include "bindings/core/v8/V8PerContextData.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/V8Window.h"
#include "bindings/core/v8/WindowProxy.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/events/Event.h"
#include "core/events/EventListener.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/NavigationScheduler.h"
#include "core/loader/ProgressTracker.h"
#include "core/plugins/PluginView.h"
#include "platform/Histogram.h"
#include "platform/TraceEvent.h"
#include "platform/UserGestureIndicator.h"
#include "platform/Widget.h"
#include "platform/v8_inspector/public/V8StackTrace.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "wtf/CurrentTime.h"
#include "wtf/StdLibExtras.h"
#include "wtf/StringExtras.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/TextPosition.h"

namespace blink {

bool ScriptController::canAccessFromCurrentOrigin(v8::Isolate* isolate, Frame* frame)
{
    if (!frame)
        return false;
    return !isolate->InContext() || BindingSecurity::shouldAllowAccessToFrame(isolate, currentDOMWindow(isolate), frame, ReportSecurityError);
}

ScriptController::ScriptController(LocalFrame* frame)
    : m_windowProxyManager(WindowProxyManager::create(*frame))
{
}

DEFINE_TRACE(ScriptController)
{
    visitor->trace(m_windowProxyManager);
}

void ScriptController::clearForClose()
{
    m_windowProxyManager->clearForClose();
    MainThreadDebugger::instance()->didClearContextsForFrame(frame());
}

void ScriptController::updateSecurityOrigin(SecurityOrigin* origin)
{
    m_windowProxyManager->mainWorldProxy()->updateSecurityOrigin(origin);
    Vector<std::pair<ScriptState*, SecurityOrigin*>> isolatedContexts;
    m_windowProxyManager->collectIsolatedContexts(isolatedContexts);
    for (auto isolatedContext : isolatedContexts)
        m_windowProxyManager->windowProxy(isolatedContext.first->world())->updateSecurityOrigin(isolatedContext.second);
}

v8::MaybeLocal<v8::Value> ScriptController::callFunction(v8::Local<v8::Function> function, v8::Local<v8::Value> receiver, int argc, v8::Local<v8::Value> info[])
{
    return ScriptController::callFunction(frame()->document(), function, receiver, argc, info, isolate());
}

v8::MaybeLocal<v8::Value> ScriptController::callFunction(ExecutionContext* context, v8::Local<v8::Function> function, v8::Local<v8::Value> receiver, int argc, v8::Local<v8::Value> info[], v8::Isolate* isolate)
{
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::callFunction(function, context, receiver, argc, info, isolate);
    return result;
}

v8::Local<v8::Value> ScriptController::executeScriptAndReturnValue(v8::Local<v8::Context> context, const ScriptSourceCode& source, AccessControlStatus accessControlStatus, double* compilationFinishTime)
{
    TRACE_EVENT1("devtools.timeline", "EvaluateScript", "data", InspectorEvaluateScriptEvent::data(frame(), source.url().getString(), source.startPosition()));
    InspectorInstrumentation::NativeBreakpoint nativeBreakpoint(frame()->document(), "scriptFirstStatement", false);

    v8::Local<v8::Value> result;
    {
        V8CacheOptions v8CacheOptions(V8CacheOptionsDefault);
        if (frame()->settings())
            v8CacheOptions = frame()->settings()->v8CacheOptions();
        if (source.resource() && !source.resource()->response().cacheStorageCacheName().isNull()) {
            switch (frame()->settings()->v8CacheStrategiesForCacheStorage()) {
            case V8CacheStrategiesForCacheStorage::None:
                v8CacheOptions = V8CacheOptionsNone;
                break;
            case V8CacheStrategiesForCacheStorage::Normal:
                v8CacheOptions = V8CacheOptionsCode;
                break;
            case V8CacheStrategiesForCacheStorage::Default:
            case V8CacheStrategiesForCacheStorage::Aggressive:
                v8CacheOptions = V8CacheOptionsAlways;
                break;
            }
        }

        // Isolate exceptions that occur when compiling and executing
        // the code. These exceptions should not interfere with
        // javascript code we might evaluate from C++ when returning
        // from here.
        v8::TryCatch tryCatch(isolate());
        tryCatch.SetVerbose(true);

        v8::Local<v8::Script> script;
        if (!v8Call(V8ScriptRunner::compileScript(source, isolate(), accessControlStatus, v8CacheOptions), script, tryCatch))
            return result;

        if (compilationFinishTime) {
            *compilationFinishTime = WTF::monotonicallyIncreasingTime();
        }
        if (!v8Call(V8ScriptRunner::runCompiledScript(isolate(), script, frame()->document()), result, tryCatch))
            return result;
    }

    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data", InspectorUpdateCountersEvent::data());

    return result;
}

bool ScriptController::initializeMainWorld()
{
    if (m_windowProxyManager->mainWorldProxy()->isContextInitialized())
        return false;
    return windowProxy(DOMWrapperWorld::mainWorld())->isContextInitialized();
}

WindowProxy* ScriptController::existingWindowProxy(DOMWrapperWorld& world)
{
    return m_windowProxyManager->existingWindowProxy(world);
}

WindowProxy* ScriptController::windowProxy(DOMWrapperWorld& world)
{
    WindowProxy* windowProxy = m_windowProxyManager->windowProxy(world);
    if (!windowProxy->isContextInitialized() && windowProxy->initializeIfNeeded() && world.isMainWorld())
        frame()->loader().dispatchDidClearWindowObjectInMainWorld();
    // FIXME: There are some situations where we can return an uninitialized
    // context. This is broken.
    return windowProxy;
}

bool ScriptController::shouldBypassMainWorldCSP()
{
    v8::HandleScope handleScope(isolate());
    v8::Local<v8::Context> context = isolate()->GetCurrentContext();
    if (context.IsEmpty() || !toDOMWindow(context))
        return false;
    DOMWrapperWorld& world = DOMWrapperWorld::current(isolate());
    return world.isIsolatedWorld() ? world.isolatedWorldHasContentSecurityPolicy() : false;
}

TextPosition ScriptController::eventHandlerPosition() const
{
    ScriptableDocumentParser* parser = frame()->document()->scriptableDocumentParser();
    if (parser)
        return parser->textPosition();
    return TextPosition::minimumPosition();
}

void ScriptController::enableEval()
{
    v8::HandleScope handleScope(isolate());
    v8::Local<v8::Context> v8Context = m_windowProxyManager->mainWorldProxy()->contextIfInitialized();
    if (v8Context.IsEmpty())
        return;
    v8Context->AllowCodeGenerationFromStrings(true);
}

void ScriptController::disableEval(const String& errorMessage)
{
    v8::HandleScope handleScope(isolate());
    v8::Local<v8::Context> v8Context = m_windowProxyManager->mainWorldProxy()->contextIfInitialized();
    if (v8Context.IsEmpty())
        return;
    v8Context->AllowCodeGenerationFromStrings(false);
    v8Context->SetErrorMessageForCodeGenerationFromStrings(v8String(isolate(), errorMessage));
}

PassRefPtr<SharedPersistent<v8::Object>> ScriptController::createPluginWrapper(Widget* widget)
{
    ASSERT(widget);

    if (!widget->isPluginView())
        return nullptr;

    v8::HandleScope handleScope(isolate());
    v8::Local<v8::Object> scriptableObject = toPluginView(widget)->scriptableObject(isolate());

    if (scriptableObject.IsEmpty())
        return nullptr;

    return SharedPersistent<v8::Object>::create(scriptableObject, isolate());
}

V8Extensions& ScriptController::registeredExtensions()
{
    DEFINE_STATIC_LOCAL(V8Extensions, extensions, ());
    return extensions;
}

void ScriptController::registerExtensionIfNeeded(v8::Extension* extension)
{
    const V8Extensions& extensions = registeredExtensions();
    for (size_t i = 0; i < extensions.size(); ++i) {
        if (extensions[i] == extension)
            return;
    }
    v8::RegisterExtension(extension);
    registeredExtensions().append(extension);
}

void ScriptController::clearWindowProxy()
{
    // V8 binding expects ScriptController::clearWindowProxy only be called
    // when a frame is loading a new page. This creates a new context for the new page.
    m_windowProxyManager->clearForNavigation();
    MainThreadDebugger::instance()->didClearContextsForFrame(frame());
}

void ScriptController::collectIsolatedContexts(Vector<std::pair<ScriptState*, SecurityOrigin*>>& result)
{
    m_windowProxyManager->collectIsolatedContexts(result);
}

void ScriptController::updateDocument()
{
    // For an uninitialized main window windowProxy, do not incur the cost of context initialization.
    if (!m_windowProxyManager->mainWorldProxy()->isGlobalInitialized())
        return;

    if (!initializeMainWorld())
        windowProxy(DOMWrapperWorld::mainWorld())->updateDocument();
}

void ScriptController::namedItemAdded(HTMLDocument* doc, const AtomicString& name)
{
    windowProxy(DOMWrapperWorld::mainWorld())->namedItemAdded(doc, name);
}

void ScriptController::namedItemRemoved(HTMLDocument* doc, const AtomicString& name)
{
    windowProxy(DOMWrapperWorld::mainWorld())->namedItemRemoved(doc, name);
}

static bool isInPrivateScriptIsolateWorld(v8::Isolate* isolate)
{
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    return !context.IsEmpty() && toDOMWindow(context) && DOMWrapperWorld::current(isolate).isPrivateScriptIsolatedWorld();
}

bool ScriptController::canExecuteScripts(ReasonForCallingCanExecuteScripts reason)
{
    // For performance reasons, we check isInPrivateScriptIsolateWorld() only if
    // canExecuteScripts is going to return false.

    if (frame()->document() && frame()->document()->isSandboxed(SandboxScripts)) {
        if (isInPrivateScriptIsolateWorld(isolate()))
            return true;
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        if (reason == AboutToExecuteScript)
            frame()->document()->addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, "Blocked script execution in '" + frame()->document()->url().elidedString() + "' because the document's frame is sandboxed and the 'allow-scripts' permission is not set."));
        return false;
    }

    if (frame()->document() && frame()->document()->isViewSource()) {
        ASSERT(frame()->document()->getSecurityOrigin()->isUnique());
        return true;
    }

    FrameLoaderClient* client = frame()->loader().client();
    if (!client)
        return false;
    Settings* settings = frame()->settings();
    const bool allowed = client->allowScript(settings && settings->scriptEnabled())
        || isInPrivateScriptIsolateWorld(isolate());
    if (!allowed && reason == AboutToExecuteScript)
        client->didNotAllowScript();
    return allowed;
}

bool ScriptController::executeScriptIfJavaScriptURL(const KURL& url)
{
    if (!protocolIsJavaScript(url))
        return false;

    bool shouldBypassMainWorldContentSecurityPolicy = ContentSecurityPolicy::shouldBypassMainWorld(frame()->document());
    if (!frame()->page()
        || (!shouldBypassMainWorldContentSecurityPolicy && !frame()->document()->contentSecurityPolicy()->allowJavaScriptURLs(frame()->document()->url(), eventHandlerPosition().m_line)))
        return true;

    bool progressNotificationsNeeded = frame()->loader().stateMachine()->isDisplayingInitialEmptyDocument() && !frame()->isLoading();
    if (progressNotificationsNeeded)
        frame()->loader().progress().progressStarted();

    Document* ownerDocument = frame()->document();

    const int javascriptSchemeLength = sizeof("javascript:") - 1;

    bool locationChangeBefore = frame()->navigationScheduler().locationChangePending();

    String decodedURL = decodeURLEscapeSequences(url.getString());
    v8::HandleScope handleScope(isolate());
    v8::Local<v8::Value> result = evaluateScriptInMainWorld(ScriptSourceCode(decodedURL.substring(javascriptSchemeLength)), NotSharableCrossOrigin, DoNotExecuteScriptWhenScriptsDisabled);

    // If executing script caused this frame to be removed from the page, we
    // don't want to try to replace its document!
    if (!frame()->page())
        return true;

    if (result.IsEmpty() || !result->IsString()) {
        if (progressNotificationsNeeded)
            frame()->loader().progress().progressCompleted();
        return true;
    }
    String scriptResult = toCoreString(v8::Local<v8::String>::Cast(result));

    // We're still in a frame, so there should be a DocumentLoader.
    ASSERT(frame()->document()->loader());
    if (!locationChangeBefore && frame()->navigationScheduler().locationChangePending())
        return true;

    frame()->loader().replaceDocumentWhileExecutingJavaScriptURL(scriptResult, ownerDocument);
    return true;
}

void ScriptController::executeScriptInMainWorld(const String& script, ExecuteScriptPolicy policy)
{
    v8::HandleScope handleScope(isolate());
    evaluateScriptInMainWorld(ScriptSourceCode(script), NotSharableCrossOrigin, policy);
}

void ScriptController::executeScriptInMainWorld(const ScriptSourceCode& sourceCode, AccessControlStatus accessControlStatus, double* compilationFinishTime)
{
    v8::HandleScope handleScope(isolate());
    evaluateScriptInMainWorld(sourceCode, accessControlStatus, DoNotExecuteScriptWhenScriptsDisabled, compilationFinishTime);
}

v8::Local<v8::Value> ScriptController::executeScriptInMainWorldAndReturnValue(const ScriptSourceCode& sourceCode, ExecuteScriptPolicy policy)
{
    return evaluateScriptInMainWorld(sourceCode, NotSharableCrossOrigin, policy);
}

v8::Local<v8::Value> ScriptController::evaluateScriptInMainWorld(const ScriptSourceCode& sourceCode, AccessControlStatus accessControlStatus, ExecuteScriptPolicy policy, double* compilationFinishTime)
{
    if (policy == DoNotExecuteScriptWhenScriptsDisabled && !canExecuteScripts(AboutToExecuteScript))
        return v8::Local<v8::Value>();

    ScriptState* scriptState = ScriptState::forMainWorld(frame());
    if (!scriptState)
        return v8::Local<v8::Value>();
    v8::EscapableHandleScope handleScope(isolate());
    ScriptState::Scope scope(scriptState);

    if (frame()->loader().stateMachine()->isDisplayingInitialEmptyDocument())
        frame()->loader().didAccessInitialDocument();

    v8::Local<v8::Value> object = executeScriptAndReturnValue(scriptState->context(), sourceCode, accessControlStatus, compilationFinishTime);

    if (object.IsEmpty())
        return v8::Local<v8::Value>();

    return handleScope.Escape(object);
}

void ScriptController::executeScriptInIsolatedWorld(int worldID, const HeapVector<ScriptSourceCode>& sources, int extensionGroup, Vector<v8::Local<v8::Value>>* results)
{
    ASSERT(worldID > 0);

    RefPtr<DOMWrapperWorld> world = DOMWrapperWorld::ensureIsolatedWorld(isolate(), worldID, extensionGroup);
    WindowProxy* isolatedWorldWindowProxy = windowProxy(*world);
    if (!isolatedWorldWindowProxy->isContextInitialized())
        return;

    ScriptState* scriptState = isolatedWorldWindowProxy->getScriptState();
    v8::Context::Scope scope(scriptState->context());
    v8::Local<v8::Array> resultArray = v8::Array::New(isolate(), sources.size());

    for (size_t i = 0; i < sources.size(); ++i) {
        v8::Local<v8::Value> evaluationResult = executeScriptAndReturnValue(scriptState->context(), sources[i]);
        if (evaluationResult.IsEmpty())
            evaluationResult = v8::Local<v8::Value>::New(isolate(), v8::Undefined(isolate()));
        if (!v8CallBoolean(resultArray->CreateDataProperty(scriptState->context(), i, evaluationResult)))
            return;
    }

    if (results) {
        for (size_t i = 0; i < resultArray->Length(); ++i) {
            v8::Local<v8::Value> value;
            if (!resultArray->Get(scriptState->context(), i).ToLocal(&value))
                return;
            results->append(value);
        }
    }
}

} // namespace blink
