/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "web/WebDevToolsFrontendImpl.h"

#include "V8InspectorFrontendHost.h"
#include "V8MouseEvent.h"
#include "V8Node.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8DOMWrapper.h"
#include "core/clipboard/Pasteboard.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/events/Event.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorFrontendHost.h"
#include "core/page/ContextMenuController.h"
#include "core/page/Page.h"
#include "platform/ContextMenuItem.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/web/WebDevToolsFrontendClient.h"
#include "public/web/WebScriptSource.h"
#include "web/InspectorFrontendClientImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "wtf/OwnPtr.h"

using namespace WebCore;

namespace blink {

class WebDevToolsFrontendImpl::InspectorFrontendResumeObserver : public ActiveDOMObject {
    WTF_MAKE_NONCOPYABLE(InspectorFrontendResumeObserver);
public:
    InspectorFrontendResumeObserver(WebDevToolsFrontendImpl* webDevToolsFrontendImpl, Document* document)
        : ActiveDOMObject(document)
        , m_webDevToolsFrontendImpl(webDevToolsFrontendImpl)
    {
        suspendIfNeeded();
    }

private:
    virtual void resume() OVERRIDE
    {
        m_webDevToolsFrontendImpl->resume();
    }

    WebDevToolsFrontendImpl* m_webDevToolsFrontendImpl;
};

WebDevToolsFrontend* WebDevToolsFrontend::create(
    WebView* view,
    WebDevToolsFrontendClient* client,
    const WebString& applicationLocale)
{
    return new WebDevToolsFrontendImpl(toWebViewImpl(view), client, applicationLocale);
}

WebDevToolsFrontendImpl::WebDevToolsFrontendImpl(
    WebViewImpl* webViewImpl,
    WebDevToolsFrontendClient* client,
    const String& applicationLocale)
    : m_webViewImpl(webViewImpl)
    , m_client(client)
    , m_applicationLocale(applicationLocale)
    , m_inspectorFrontendDispatchTimer(this, &WebDevToolsFrontendImpl::maybeDispatch)
{
    m_webViewImpl->page()->inspectorController().setInspectorFrontendClient(adoptPtr(new InspectorFrontendClientImpl(m_webViewImpl->page(), m_client, this)));
}

WebDevToolsFrontendImpl::~WebDevToolsFrontendImpl()
{
}

void WebDevToolsFrontendImpl::dispatchOnInspectorFrontend(const WebString& message)
{
    m_messages.append(message);
    maybeDispatch(0);
}

void WebDevToolsFrontendImpl::resume()
{
    // We should call maybeDispatch asynchronously here because we are not allowed to update activeDOMObjects list in
    // resume (See ExecutionContext::resumeActiveDOMObjects).
    if (!m_inspectorFrontendDispatchTimer.isActive())
        m_inspectorFrontendDispatchTimer.startOneShot(0, FROM_HERE);
}

void WebDevToolsFrontendImpl::maybeDispatch(WebCore::Timer<WebDevToolsFrontendImpl>*)
{
    while (!m_messages.isEmpty()) {
        Document* document = m_webViewImpl->page()->deprecatedLocalMainFrame()->document();
        if (document->activeDOMObjectsAreSuspended()) {
            m_inspectorFrontendResumeObserver = adoptPtr(new InspectorFrontendResumeObserver(this, document));
            return;
        }
        m_inspectorFrontendResumeObserver.clear();
        doDispatchOnInspectorFrontend(m_messages.takeFirst());
    }
}

void WebDevToolsFrontendImpl::doDispatchOnInspectorFrontend(const WebString& message)
{
    WebLocalFrameImpl* frame = m_webViewImpl->mainFrameImpl();
    if (!frame->frame())
        return;
    v8::Isolate* isolate = toIsolate(frame->frame());
    ScriptState* scriptState = ScriptState::forMainWorld(frame->frame());
    ScriptState::Scope scope(scriptState);
    v8::Handle<v8::Value> inspectorFrontendApiValue = scriptState->context()->Global()->Get(v8::String::NewFromUtf8(isolate, "InspectorFrontendAPI"));
    if (!inspectorFrontendApiValue->IsObject())
        return;
    v8::Handle<v8::Object> dispatcherObject = v8::Handle<v8::Object>::Cast(inspectorFrontendApiValue);
    v8::Handle<v8::Value> dispatchFunction = dispatcherObject->Get(v8::String::NewFromUtf8(isolate, "dispatchMessage"));
    // The frame might have navigated away from the front-end page (which is still weird),
    // OR the older version of frontend might have a dispatch method in a different place.
    // FIXME(kaznacheev): Remove when Chrome for Android M18 is retired.
    if (!dispatchFunction->IsFunction()) {
        v8::Handle<v8::Value> inspectorBackendApiValue = scriptState->context()->Global()->Get(v8::String::NewFromUtf8(isolate, "InspectorBackend"));
        if (!inspectorBackendApiValue->IsObject())
            return;
        dispatcherObject = v8::Handle<v8::Object>::Cast(inspectorBackendApiValue);
        dispatchFunction = dispatcherObject->Get(v8::String::NewFromUtf8(isolate, "dispatch"));
        if (!dispatchFunction->IsFunction())
            return;
    }
    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(dispatchFunction);
    Vector< v8::Handle<v8::Value> > args;
    args.append(v8String(isolate, message));
    v8::TryCatch tryCatch;
    tryCatch.SetVerbose(true);
    ScriptController::callFunction(frame->frame()->document(), function, dispatcherObject, args.size(), args.data(), isolate);
}

} // namespace blink
