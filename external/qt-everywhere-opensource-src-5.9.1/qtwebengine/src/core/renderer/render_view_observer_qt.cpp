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

#include "renderer/render_view_observer_qt.h"

#include "common/qt_messages.h"

#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebFrameContentDumper.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

RenderViewObserverQt::RenderViewObserverQt(
        content::RenderView* render_view,
        web_cache::WebCacheImpl* web_cache_impl)
    : content::RenderViewObserver(render_view)
    , m_web_cache_impl(web_cache_impl)
{
}

void RenderViewObserverQt::onFetchDocumentMarkup(quint64 requestId)
{
    blink::WebString markup;
    if (render_view()->GetWebView()->mainFrame()->isWebLocalFrame())
        markup = blink::WebFrameContentDumper::dumpAsMarkup(
                    static_cast<blink::WebLocalFrame*>(render_view()->GetWebView()->mainFrame()));
    Send(new RenderViewObserverHostQt_DidFetchDocumentMarkup(routing_id(), requestId, markup));
}

void RenderViewObserverQt::onFetchDocumentInnerText(quint64 requestId)
{
    blink::WebString text;
    if (render_view()->GetWebView()->mainFrame()->isWebLocalFrame())
        text = blink::WebFrameContentDumper::dumpWebViewAsText(
                    render_view()->GetWebView(),
                    std::numeric_limits<std::size_t>::max());
    Send(new RenderViewObserverHostQt_DidFetchDocumentInnerText(routing_id(), requestId, text));
}

void RenderViewObserverQt::onSetBackgroundColor(quint32 color)
{
    render_view()->GetWebFrameWidget()->setBaseBackgroundColor(color);
}

bool RenderViewObserverQt::OnMessageReceived(const IPC::Message& message)
{
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(RenderViewObserverQt, message)
        IPC_MESSAGE_HANDLER(RenderViewObserverQt_FetchDocumentMarkup, onFetchDocumentMarkup)
        IPC_MESSAGE_HANDLER(RenderViewObserverQt_FetchDocumentInnerText, onFetchDocumentInnerText)
        IPC_MESSAGE_HANDLER(RenderViewObserverQt_SetBackgroundColor, onSetBackgroundColor)
        IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
}

void RenderViewObserverQt::Navigate(const GURL &)
{
    if (m_web_cache_impl)
        m_web_cache_impl->ExecutePendingClearCache();
}
