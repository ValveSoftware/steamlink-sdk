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

#include "qt_render_view_observer_host.h"

#include "common/qt_messages.h"
#include "content/public/browser/web_contents.h"
#include "render_widget_host_view_qt.h"
#include "type_conversion.h"
#include "web_contents_adapter_client.h"

QtRenderViewObserverHost::QtRenderViewObserverHost(content::WebContents *webContents, WebContentsAdapterClient *adapterClient)
    : content::WebContentsObserver(webContents)
    , m_adapterClient(adapterClient)
{
}

void QtRenderViewObserverHost::fetchDocumentMarkup(quint64 requestId)
{
    Send(new QtRenderViewObserver_FetchDocumentMarkup(routing_id(), requestId));
}

void QtRenderViewObserverHost::fetchDocumentInnerText(quint64 requestId)
{
    Send(new QtRenderViewObserver_FetchDocumentInnerText(routing_id(), requestId));
}

bool QtRenderViewObserverHost::OnMessageReceived(const IPC::Message& message)
{
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(QtRenderViewObserverHost, message)
        IPC_MESSAGE_HANDLER(QtRenderViewObserverHost_DidFetchDocumentMarkup,
                            onDidFetchDocumentMarkup)
        IPC_MESSAGE_HANDLER(QtRenderViewObserverHost_DidFetchDocumentInnerText,
                            onDidFetchDocumentInnerText)
        IPC_MESSAGE_HANDLER(QtRenderViewObserverHost_DidFirstVisuallyNonEmptyLayout,
                            onDidFirstVisuallyNonEmptyLayout)
        IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;

}

void QtRenderViewObserverHost::onDidFetchDocumentMarkup(quint64 requestId, const base::string16& markup)
{
    m_adapterClient->didFetchDocumentMarkup(requestId, toQt(markup));
}

void QtRenderViewObserverHost::onDidFetchDocumentInnerText(quint64 requestId, const base::string16& innerText)
{
    m_adapterClient->didFetchDocumentInnerText(requestId, toQt(innerText));
}

void QtRenderViewObserverHost::onDidFirstVisuallyNonEmptyLayout()
{
    RenderWidgetHostViewQt *rwhv = static_cast<RenderWidgetHostViewQt*>(web_contents()->GetRenderWidgetHostView());
    if (rwhv)
        rwhv->didFirstVisuallyNonEmptyLayout();
}
