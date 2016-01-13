/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "renderer/qt_render_view_observer.h"

#include "common/qt_messages.h"

#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

QtRenderViewObserver::QtRenderViewObserver(content::RenderView* render_view)
    : content::RenderViewObserver(render_view)
{
}

void QtRenderViewObserver::onFetchDocumentMarkup(quint64 requestId)
{
    Send(new QtRenderViewObserverHost_DidFetchDocumentMarkup(
        routing_id(),
        requestId,
        render_view()->GetWebView()->mainFrame()->contentAsMarkup()));
}

void QtRenderViewObserver::onFetchDocumentInnerText(quint64 requestId)
{
    Send(new QtRenderViewObserverHost_DidFetchDocumentInnerText(
        routing_id(),
        requestId,
        render_view()->GetWebView()->mainFrame()->contentAsText(std::numeric_limits<std::size_t>::max())));
}

void QtRenderViewObserver::OnFirstVisuallyNonEmptyLayout()
{
    Send(new QtRenderViewObserverHost_DidFirstVisuallyNonEmptyLayout(routing_id()));
}

bool QtRenderViewObserver::OnMessageReceived(const IPC::Message& message)
{
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(QtRenderViewObserver, message)
        IPC_MESSAGE_HANDLER(QtRenderViewObserver_FetchDocumentMarkup, onFetchDocumentMarkup)
        IPC_MESSAGE_HANDLER(QtRenderViewObserver_FetchDocumentInnerText, onFetchDocumentInnerText)
        IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
}
