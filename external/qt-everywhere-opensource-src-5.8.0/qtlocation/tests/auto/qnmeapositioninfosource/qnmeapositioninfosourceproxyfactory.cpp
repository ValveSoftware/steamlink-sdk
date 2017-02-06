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

#include "qnmeapositioninfosourceproxyfactory.h"
#include "../utils/qlocationtestutils_p.h"

#include <QIODevice>
#include <QTcpServer>
#include <QTcpSocket>


QNmeaPositionInfoSourceProxy::QNmeaPositionInfoSourceProxy(QNmeaPositionInfoSource *source, QIODevice *outDevice)
    : m_source(source),
        m_outDevice(outDevice)
{
}

QNmeaPositionInfoSourceProxy::~QNmeaPositionInfoSourceProxy()
{
    m_outDevice->close();
    delete m_outDevice;
}

QGeoPositionInfoSource *QNmeaPositionInfoSourceProxy::source() const
{
    return m_source;
}

void QNmeaPositionInfoSourceProxy::feedUpdate(const QDateTime &dt)
{
    m_outDevice->write(QLocationTestUtils::createRmcSentence(dt).toLatin1());
}

void QNmeaPositionInfoSourceProxy::feedBytes(const QByteArray &bytes)
{
    m_outDevice->write(bytes);
}


QNmeaPositionInfoSourceProxyFactory::QNmeaPositionInfoSourceProxyFactory()
    : m_server(new QTcpServer(this))
{
    bool b = m_server->listen(QHostAddress::LocalHost);
    Q_ASSERT(b);
}

QNmeaPositionInfoSourceProxy *QNmeaPositionInfoSourceProxyFactory::createProxy(QNmeaPositionInfoSource *source)
{
    QTcpSocket *client = new QTcpSocket;
    client->connectToHost(m_server->serverAddress(), m_server->serverPort());
    qDebug() << "listening on" << m_server->serverAddress() << m_server->serverPort();
    bool b = m_server->waitForNewConnection(15000);
    if (!b)
        qWarning() << "Server didin't receive new connection";
    b = client->waitForConnected();
    if (!b)
        qWarning() << "Client could not connect to server";

    //QNmeaPositionInfoSource *source = new QNmeaPositionInfoSource(m_mode);
    QIODevice *device = m_server->nextPendingConnection();
    if (!device)
        qWarning() << "Missing pending connection. Test is going to fail.";
    else
        qWarning() << "Received pending connection:" << device << b;
    source->setDevice(device);
    Q_ASSERT(source->device() != 0);
    QNmeaPositionInfoSourceProxy *proxy = new QNmeaPositionInfoSourceProxy(source, client);
    proxy->setParent(source);
    return proxy;
}
