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

#include "web/WebDevToolsFrontendImpl.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8DevToolsHost.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/DevToolsHost.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebString.h"
#include "public/web/WebDevToolsFrontendClient.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

WebDevToolsFrontend* WebDevToolsFrontend::create(
    WebLocalFrame* frame,
    WebDevToolsFrontendClient* client)
{
    return new WebDevToolsFrontendImpl(toWebLocalFrameImpl(frame), client);
}

WebDevToolsFrontendImpl::WebDevToolsFrontendImpl(
    WebLocalFrameImpl* webFrame,
    WebDevToolsFrontendClient* client)
    : m_webFrame(webFrame)
    , m_client(client)
{
    m_webFrame->setDevToolsFrontend(this);
    m_webFrame->frame()->host()->setDefaultPageScaleLimits(1.f, 1.f);
}

WebDevToolsFrontendImpl::~WebDevToolsFrontendImpl()
{
    if (m_devtoolsHost)
        m_devtoolsHost->disconnectClient();
}

void WebDevToolsFrontendImpl::didClearWindowObject(WebLocalFrameImpl* frame)
{
    if (m_webFrame == frame) {
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        ScriptState* scriptState = ScriptState::forMainWorld(m_webFrame->frame());
        DCHECK(scriptState);
        ScriptState::Scope scope(scriptState);

        if (m_devtoolsHost)
            m_devtoolsHost->disconnectClient();
        m_devtoolsHost = DevToolsHost::create(this, m_webFrame->frame());
        v8::Local<v8::Object> global = scriptState->context()->Global();
        v8::Local<v8::Value> devtoolsHostObj = toV8(m_devtoolsHost.get(), global, scriptState->isolate());
        DCHECK(!devtoolsHostObj.IsEmpty());
        global->Set(v8AtomicString(isolate, "DevToolsHost"), devtoolsHostObj);
    }

    if (m_injectedScriptForOrigin.isEmpty())
        return;

    String origin = frame->getSecurityOrigin().toString();
    String script = m_injectedScriptForOrigin.get(origin);
    if (script.isEmpty())
        return;
    static int s_lastScriptId = 0;
    StringBuilder scriptWithId;
    scriptWithId.append(script);
    scriptWithId.append('(');
    scriptWithId.appendNumber(++s_lastScriptId);
    scriptWithId.append(')');
    frame->frame()->script().executeScriptInMainWorld(scriptWithId.toString());
}

void WebDevToolsFrontendImpl::sendMessageToEmbedder(const String& message)
{
    if (m_client)
        m_client->sendMessageToEmbedder(message);
}

bool WebDevToolsFrontendImpl::isUnderTest()
{
    return m_client ? m_client->isUnderTest() : false;
}

void WebDevToolsFrontendImpl::showContextMenu(LocalFrame* targetFrame, float x, float y, ContextMenuProvider* menuProvider)
{
    WebLocalFrameImpl::fromFrame(targetFrame)->viewImpl()->showContextMenuAtPoint(x, y, menuProvider);
}

void WebDevToolsFrontendImpl::setInjectedScriptForOrigin(const String& origin, const String& source)
{
    m_injectedScriptForOrigin.set(origin, source);
}

} // namespace blink
