/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/inspector/DevToolsHost.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/clipboard/Pasteboard.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "core/events/EventTarget.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/inspector/InspectorFrontendClient.h"
#include "core/layout/LayoutTheme.h"
#include "core/loader/FrameLoader.h"
#include "core/page/ContextMenuController.h"
#include "core/page/ContextMenuProvider.h"
#include "core/page/Page.h"
#include "platform/ContextMenu.h"
#include "platform/ContextMenuItem.h"
#include "platform/HostWindow.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/SharedBuffer.h"
#include "platform/UserGestureIndicator.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"

namespace blink {

class FrontendMenuProvider final : public ContextMenuProvider {
public:
    static FrontendMenuProvider* create(DevToolsHost* devtoolsHost, const Vector<ContextMenuItem>& items)
    {
        return new FrontendMenuProvider(devtoolsHost, items);
    }

    ~FrontendMenuProvider() override
    {
        // Verify that this menu provider has been detached.
        ASSERT(!m_devtoolsHost);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_devtoolsHost);
        ContextMenuProvider::trace(visitor);
    }

    void disconnect()
    {
        m_devtoolsHost = nullptr;
    }

    void contextMenuCleared() override
    {
        if (m_devtoolsHost) {
            m_devtoolsHost->evaluateScript("DevToolsAPI.contextMenuCleared()");
            m_devtoolsHost->clearMenuProvider();
            m_devtoolsHost = nullptr;
        }
        m_items.clear();
    }

    void populateContextMenu(ContextMenu* menu) override
    {
        for (size_t i = 0; i < m_items.size(); ++i)
            menu->appendItem(m_items[i]);
    }

    void contextMenuItemSelected(const ContextMenuItem* item) override
    {
        if (!m_devtoolsHost)
            return;
        int itemNumber = item->action() - ContextMenuItemBaseCustomTag;
        m_devtoolsHost->evaluateScript("DevToolsAPI.contextMenuItemSelected(" + String::number(itemNumber) + ")");
    }

private:
    FrontendMenuProvider(DevToolsHost* devtoolsHost, const Vector<ContextMenuItem>& items)
        : m_devtoolsHost(devtoolsHost)
        , m_items(items)
    {
    }

    Member<DevToolsHost> m_devtoolsHost;
    Vector<ContextMenuItem> m_items;
};

DevToolsHost::DevToolsHost(InspectorFrontendClient* client, LocalFrame* frontendFrame)
    : m_client(client)
    , m_frontendFrame(frontendFrame)
    , m_menuProvider(nullptr)
{
}

DevToolsHost::~DevToolsHost()
{
    ASSERT(!m_client);
}

DEFINE_TRACE(DevToolsHost)
{
    visitor->trace(m_frontendFrame);
    visitor->trace(m_menuProvider);
}

void DevToolsHost::evaluateScript(const String& expression)
{
    if (ScriptForbiddenScope::isScriptForbidden())
        return;
    if (!m_frontendFrame)
        return;
    ScriptState* scriptState = ScriptState::forMainWorld(m_frontendFrame);
    if (!scriptState)
        return;
    ScriptState::Scope scope(scriptState);
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    v8::MicrotasksScope microtasks(scriptState->isolate(), v8::MicrotasksScope::kRunMicrotasks);
    v8::Local<v8::String> source = v8AtomicString(scriptState->isolate(), expression.utf8().data());
    V8ScriptRunner::compileAndRunInternalScript(source, scriptState->isolate(), String(), TextPosition());
}

void DevToolsHost::disconnectClient()
{
    m_client = 0;
    if (m_menuProvider) {
        m_menuProvider->disconnect();
        m_menuProvider = nullptr;
    }
    m_frontendFrame = nullptr;
}

float DevToolsHost::zoomFactor()
{
    if (!m_frontendFrame)
        return 1;
    float zoomFactor = m_frontendFrame->pageZoomFactor();
    // Cancel the device scale factor applied to the zoom factor in
    // use-zoom-for-dsf mode.
    const HostWindow* hostWindow = m_frontendFrame->view()->getHostWindow();
    float windowToViewportRatio = hostWindow->windowToViewportScalar(1.0f);
    return zoomFactor / windowToViewportRatio;
}

void DevToolsHost::setInjectedScriptForOrigin(const String& origin, const String& script)
{
    if (m_client)
        m_client->setInjectedScriptForOrigin(origin, script);
}

void DevToolsHost::copyText(const String& text)
{
    Pasteboard::generalPasteboard()->writePlainText(text, Pasteboard::CannotSmartReplace);
}

static String escapeUnicodeNonCharacters(const String& str)
{
    const UChar nonChar = 0xD800;

    unsigned i = 0;
    while (i < str.length() && str[i] < nonChar)
        ++i;
    if (i == str.length())
        return str;

    StringBuilder dst;
    dst.append(str, 0, i);
    for (; i < str.length(); ++i) {
        UChar c = str[i];
        if (c >= nonChar) {
            unsigned symbol = static_cast<unsigned>(c);
            String symbolCode = String::format("\\u%04X", symbol);
            dst.append(symbolCode);
        } else {
            dst.append(c);
        }
    }
    return dst.toString();
}

void DevToolsHost::sendMessageToEmbedder(const String& message)
{
    if (m_client)
        m_client->sendMessageToEmbedder(escapeUnicodeNonCharacters(message));
}

void DevToolsHost::showContextMenu(LocalFrame* targetFrame, float x, float y, const Vector<ContextMenuItem>& items)
{
    ASSERT(m_frontendFrame);
    FrontendMenuProvider* menuProvider = FrontendMenuProvider::create(this, items);
    m_menuProvider = menuProvider;
    float zoom = targetFrame->pageZoomFactor();
    if (m_client)
        m_client->showContextMenu(targetFrame, x * zoom, y * zoom, menuProvider);
}

String DevToolsHost::getSelectionBackgroundColor()
{
    return LayoutTheme::theme().activeSelectionBackgroundColor().serialized();
}

String DevToolsHost::getSelectionForegroundColor()
{
    return LayoutTheme::theme().activeSelectionForegroundColor().serialized();
}

bool DevToolsHost::isUnderTest()
{
    return m_client && m_client->isUnderTest();
}

bool DevToolsHost::isHostedMode()
{
    return false;
}

} // namespace blink
