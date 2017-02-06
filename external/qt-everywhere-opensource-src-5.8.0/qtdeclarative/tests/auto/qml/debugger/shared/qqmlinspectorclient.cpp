/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qqmlinspectorclient.h"

#include <private/qpacket_p.h>
#include <private/qqmldebugconnection_p.h>
#include <QtCore/qdebug.h>

QQmlInspectorClient::QQmlInspectorClient(QQmlDebugConnection *connection) :
    QQmlDebugClient(QLatin1String("QmlInspector"), connection),
    m_lastRequestId(-1)
{
}

int QQmlInspectorClient::setInspectToolEnabled(bool enabled)
{
    QPacket ds(connection()->currentDataStreamVersion());
    ds << QByteArray("request") << ++m_lastRequestId
       << QByteArray(enabled ? "enable" : "disable");

    sendMessage(ds.data());
    return m_lastRequestId;
}

int QQmlInspectorClient::setShowAppOnTop(bool showOnTop)
{
    QPacket ds(connection()->currentDataStreamVersion());
    ds << QByteArray("request") << ++m_lastRequestId
       << QByteArray("showAppOnTop") << showOnTop;

    sendMessage(ds.data());
    return m_lastRequestId;
}

int QQmlInspectorClient::setAnimationSpeed(qreal speed)
{
    QPacket ds(connection()->currentDataStreamVersion());
    ds << QByteArray("request") << ++m_lastRequestId
       << QByteArray("setAnimationSpeed") << speed;

    sendMessage(ds.data());
    return m_lastRequestId;
}

int QQmlInspectorClient::select(const QList<int> &objectIds)
{
    QPacket ds(connection()->currentDataStreamVersion());
    ds << QByteArray("request") << ++m_lastRequestId
       << QByteArray("select") << objectIds;

    sendMessage(ds.data());
    return m_lastRequestId;
}

int QQmlInspectorClient::createObject(const QString &qml, int parentId, const QStringList &imports,
                                      const QString &filename)
{
    QPacket ds(connection()->currentDataStreamVersion());
    ds << QByteArray("request") << ++m_lastRequestId
       << QByteArray("createObject") << qml << parentId << imports << filename;
    sendMessage(ds.data());
    return m_lastRequestId;
}

int QQmlInspectorClient::moveObject(int childId, int newParentId)
{
    QPacket ds(connection()->currentDataStreamVersion());
    ds << QByteArray("request") << ++m_lastRequestId
       << QByteArray("moveObject") << childId << newParentId;
    sendMessage(ds.data());
    return m_lastRequestId;
}

int QQmlInspectorClient::destroyObject(int objectId)
{
    QPacket ds(connection()->currentDataStreamVersion());
    ds << QByteArray("request") << ++m_lastRequestId
       << QByteArray("destroyObject") << objectId;
    sendMessage(ds.data());
    return m_lastRequestId;
}

void QQmlInspectorClient::messageReceived(const QByteArray &message)
{
    QPacket ds(connection()->currentDataStreamVersion(), message);
    QByteArray type;
    ds >> type;

    if (type != QByteArray("response")) {
        qDebug() << "Unhandled message of type" << type;
        return;
    }

    int responseId;
    bool result;
    ds >> responseId >> result;
    emit responseReceived(responseId, result);
}
