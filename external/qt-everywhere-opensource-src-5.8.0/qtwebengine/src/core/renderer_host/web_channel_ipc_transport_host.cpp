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

#include "web_channel_ipc_transport_host.h"

#include "base/strings/string16.h"

#include "common/qt_messages.h"
#include "type_conversion.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace QtWebEngineCore {

WebChannelIPCTransportHost::WebChannelIPCTransportHost(content::WebContents *contents, uint worldId, QObject *parent)
    : QWebChannelAbstractTransport(parent)
    , content::WebContentsObserver(contents)
    , m_worldId(worldId)
{
    Send(new WebChannelIPCTransport_Install(routing_id(), m_worldId));
}

WebChannelIPCTransportHost::~WebChannelIPCTransportHost()
{
}

void WebChannelIPCTransportHost::RenderViewHostChanged(content::RenderViewHost *, content::RenderViewHost *)
{
    // This means that we were moved into a different RenderView, possibly in a different
    // render process and that we lost our WebChannelIPCTransport object and its state.
    Send(new WebChannelIPCTransport_Install(routing_id(), m_worldId));
}

void WebChannelIPCTransportHost::setWorldId(uint worldId)
{
    if (worldId == m_worldId)
        return;
    Send(new WebChannelIPCTransport_Uninstall(routing_id(), m_worldId));
    m_worldId = worldId;
    Send(new WebChannelIPCTransport_Install(routing_id(), m_worldId));
}

void WebChannelIPCTransportHost::sendMessage(const QJsonObject &message)
{
    QJsonDocument doc(message);
    int size = 0;
    const char *rawData = doc.rawData(&size);
    Send(new WebChannelIPCTransport_Message(routing_id(), std::vector<char>(rawData, rawData + size), m_worldId));
}

void WebChannelIPCTransportHost::onWebChannelMessage(const std::vector<char> &message)
{
    QJsonDocument doc = QJsonDocument::fromRawData(message.data(), message.size(), QJsonDocument::BypassValidation);
    Q_ASSERT(doc.isObject());
    Q_EMIT messageReceived(doc.object(), this);
}

bool WebChannelIPCTransportHost::OnMessageReceived(const IPC::Message &message)
{
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(WebChannelIPCTransportHost, message)
        IPC_MESSAGE_HANDLER(WebChannelIPCTransportHost_SendMessage, onWebChannelMessage)
        IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
}

} // namespace
