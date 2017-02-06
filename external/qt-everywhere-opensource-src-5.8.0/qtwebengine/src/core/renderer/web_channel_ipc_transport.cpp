/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/web_channel_ipc_transport.h"

#include "common/qt_messages.h"

#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

#include <QJsonDocument>

namespace QtWebEngineCore {

class WebChannelTransport : public gin::Wrappable<WebChannelTransport> {
public:
    static gin::WrapperInfo kWrapperInfo;
    static void Install(blink::WebFrame *frame, uint worldId);
    static void Uninstall(blink::WebFrame *frame, uint worldId);
private:
    content::RenderView *GetRenderView(v8::Isolate *isolate);
    WebChannelTransport() { }
    virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(v8::Isolate *isolate) override;

    void NativeQtSendMessage(gin::Arguments *args)
    {
        content::RenderView *renderView = GetRenderView(args->isolate());
        if (!renderView || args->Length() != 1)
            return;
        v8::Handle<v8::Value> val;
        args->GetNext(&val);
        if (!val->IsString() && !val->IsStringObject())
            return;
        v8::String::Utf8Value utf8(val->ToString());

        QByteArray valueData(*utf8, utf8.length());
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(valueData, &error);
        if (error.error != QJsonParseError::NoError)
            qWarning("%s %d: Parsing error: %s",__FILE__, __LINE__, qPrintable(error.errorString()));
        int size = 0;
        const char *rawData = doc.rawData(&size);
        renderView->Send(new WebChannelIPCTransportHost_SendMessage(renderView->GetRoutingID(), std::vector<char>(rawData, rawData + size)));
    }

    DISALLOW_COPY_AND_ASSIGN(WebChannelTransport);
};

gin::WrapperInfo WebChannelTransport::kWrapperInfo = { gin::kEmbedderNativeGin };

void WebChannelTransport::Install(blink::WebFrame *frame, uint worldId)
{
    v8::Isolate *isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Context> context;
    if (worldId == 0)
        context = frame->mainWorldScriptContext();
    else
        context = frame->toWebLocalFrame()->isolatedWorldScriptContext(worldId, 0);
    v8::Context::Scope contextScope(context);

    gin::Handle<WebChannelTransport> transport = gin::CreateHandle(isolate, new WebChannelTransport);
    v8::Handle<v8::Object> global = context->Global();
    v8::Handle<v8::Object> qt = global->Get(gin::StringToV8(isolate, "qt"))->ToObject();
    if (qt.IsEmpty()) {
        qt = v8::Object::New(isolate);
        global->Set(gin::StringToV8(isolate, "qt"), qt);
    }
    qt->Set(gin::StringToV8(isolate, "webChannelTransport"), transport.ToV8());
}

void WebChannelTransport::Uninstall(blink::WebFrame *frame, uint worldId)
{
    v8::Isolate *isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Context> context;
    if (worldId == 0)
        context = frame->mainWorldScriptContext();
    else
        context = frame->toWebLocalFrame()->isolatedWorldScriptContext(worldId, 0);
    v8::Context::Scope contextScope(context);

    v8::Handle<v8::Object> global(context->Global());
    v8::Handle<v8::Object> qt = global->Get(gin::StringToV8(isolate, "qt"))->ToObject();
    if (qt.IsEmpty())
        return;
    qt->Delete(gin::StringToV8(isolate, "webChannelTransport"));
}

gin::ObjectTemplateBuilder WebChannelTransport::GetObjectTemplateBuilder(v8::Isolate *isolate)
{
    return gin::Wrappable<WebChannelTransport>::GetObjectTemplateBuilder(isolate).SetMethod("send", &WebChannelTransport::NativeQtSendMessage);
}

content::RenderView *WebChannelTransport::GetRenderView(v8::Isolate *isolate)
{
    blink::WebLocalFrame *webframe = blink::WebLocalFrame::frameForContext(isolate->GetCurrentContext());
    DCHECK(webframe) << "There should be an active frame since we just got a native function called.";
    if (!webframe)
        return 0;

    blink::WebView *webview = webframe->view();
    if (!webview)
        return 0;  // can happen during closing

    return content::RenderView::FromWebView(webview);
}

WebChannelIPCTransport::WebChannelIPCTransport(content::RenderView *renderView)
    : content::RenderViewObserver(renderView)
    , content::RenderViewObserverTracker<WebChannelIPCTransport>(renderView)
    , m_installed(false)
    , m_installedWorldId(0)
{
}

void WebChannelIPCTransport::RunScriptsAtDocumentStart(content::RenderFrame *render_frame)
{
    // JavaScript run before this point doesn't stick, and needs to be redone.
    // ### FIXME: we should try no even installing before
    if (m_installed && render_frame->IsMainFrame())
        WebChannelTransport::Install(render_frame->GetWebFrame(), m_installedWorldId);
}


void WebChannelIPCTransport::installWebChannel(uint worldId)
{
    blink::WebView *webView = render_view()->GetWebView();
    if (!webView)
        return;
    WebChannelTransport::Install(webView->mainFrame(), worldId);
    m_installed = true;
    m_installedWorldId = worldId;
}

void WebChannelIPCTransport::uninstallWebChannel(uint worldId)
{
    Q_ASSERT(worldId = m_installedWorldId);
    blink::WebView *webView = render_view()->GetWebView();
    if (!webView)
        return;
    WebChannelTransport::Uninstall(webView->mainFrame(), worldId);
    m_installed = false;
}

void WebChannelIPCTransport::dispatchWebChannelMessage(const std::vector<char> &binaryJSON, uint worldId)
{
    blink::WebView *webView = render_view()->GetWebView();
    if (!webView)
        return;

    QJsonDocument doc = QJsonDocument::fromRawData(binaryJSON.data(), binaryJSON.size(), QJsonDocument::BypassValidation);
    Q_ASSERT(doc.isObject());
    QByteArray json = doc.toJson(QJsonDocument::Compact);

    v8::Isolate *isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);
    blink::WebFrame *frame = webView->mainFrame();
    v8::Handle<v8::Context> context;
    if (worldId == 0)
        context = frame->mainWorldScriptContext();
    else
        context = frame->toWebLocalFrame()->isolatedWorldScriptContext(worldId, 0);
    v8::Context::Scope contextScope(context);

    v8::Handle<v8::Object> global(context->Global());
    v8::Handle<v8::Value> qtObjectValue(global->Get(gin::StringToV8(isolate, "qt")));
    if (!qtObjectValue->IsObject())
        return;
    v8::Handle<v8::Value> webChannelObjectValue(qtObjectValue->ToObject()->Get(gin::StringToV8(isolate, "webChannelTransport")));
    if (!webChannelObjectValue->IsObject())
        return;
    v8::Handle<v8::Value> onmessageCallbackValue(webChannelObjectValue->ToObject()->Get(gin::StringToV8(isolate, "onmessage")));
    if (!onmessageCallbackValue->IsFunction()) {
        qWarning("onmessage is not a callable property of qt.webChannelTransport. Some things might not work as expected.");
        return;
    }

    v8::Handle<v8::Object> messageObject(v8::Object::New(isolate));
    v8::Maybe<bool> wasSet = messageObject->DefineOwnProperty(
                context,
                v8::String::NewFromUtf8(isolate, "data"),
                v8::String::NewFromUtf8(isolate, json.constData(), v8::String::kNormalString, json.size()),
                v8::PropertyAttribute(v8::ReadOnly | v8::DontDelete));
    Q_ASSERT(!wasSet.IsNothing() && wasSet.FromJust());

    v8::Handle<v8::Function> callback = v8::Handle<v8::Function>::Cast(onmessageCallbackValue);
    const int argc = 1;
    v8::Handle<v8::Value> argv[argc];
    argv[0] = messageObject;
    frame->callFunctionEvenIfScriptDisabled(callback, webChannelObjectValue->ToObject(), argc, argv);
}

bool WebChannelIPCTransport::OnMessageReceived(const IPC::Message &message)
{
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(WebChannelIPCTransport, message)
        IPC_MESSAGE_HANDLER(WebChannelIPCTransport_Install, installWebChannel)
        IPC_MESSAGE_HANDLER(WebChannelIPCTransport_Uninstall, uninstallWebChannel)
        IPC_MESSAGE_HANDLER(WebChannelIPCTransport_Message, dispatchWebChannelMessage)
        IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
}

} // namespace
