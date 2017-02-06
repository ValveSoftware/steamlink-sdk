/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include <QtCore/QFileInfo>
#include <QtCore/QStringList>

#include <QtNetwork/QHostAddress>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>

#include "qdesigner.h"
#include "qdesigner_server.h"

#include <qevent.h>

QT_BEGIN_NAMESPACE

// ### review

QDesignerServer::QDesignerServer(QObject *parent)
    : QObject(parent)
{
    m_socket = 0;
    m_server = new QTcpServer(this);
    m_server->listen(QHostAddress::LocalHost, 0);
    if (m_server->isListening())
    {
        connect(m_server, &QTcpServer::newConnection,
                this, &QDesignerServer::handleNewConnection);
    }
}

QDesignerServer::~QDesignerServer()
{
}

quint16 QDesignerServer::serverPort() const
{
    return m_server ? m_server->serverPort() : 0;
}

void QDesignerServer::sendOpenRequest(int port, const QStringList &files)
{
    QTcpSocket *sSocket = new QTcpSocket();
    sSocket->connectToHost(QHostAddress::LocalHost, port);
    if(sSocket->waitForConnected(3000))
    {
        foreach(const QString &file, files)
        {
            QFileInfo fi(file);
            sSocket->write(fi.absoluteFilePath().toUtf8() + '\n');
        }
        sSocket->waitForBytesWritten(3000);
        sSocket->close();
    }
    delete sSocket;
}

void QDesignerServer::readFromClient()
{
    while (m_socket->canReadLine()) {
        QString file = QString::fromUtf8(m_socket->readLine());
        if (!file.isNull()) {
            file.remove(QLatin1Char('\n'));
            file.remove(QLatin1Char('\r'));
            qDesigner->postEvent(qDesigner, new QFileOpenEvent(file));
        }
    }
}

void QDesignerServer::socketClosed()
{
    m_socket = 0;
}

void QDesignerServer::handleNewConnection()
{
    // no need for more than one connection
    if (m_socket == 0) {
        m_socket = m_server->nextPendingConnection();
        connect(m_socket, &QTcpSocket::readyRead,
                this, &QDesignerServer::readFromClient);
        connect(m_socket, &QTcpSocket::disconnected,
                this, &QDesignerServer::socketClosed);
    }
}


QDesignerClient::QDesignerClient(quint16 port, QObject *parent)
: QObject(parent)
{
    m_socket = new QTcpSocket(this);
    m_socket->connectToHost(QHostAddress::LocalHost, port);
    connect(m_socket, &QTcpSocket::readyRead,
                this, &QDesignerClient::readFromSocket);

}

QDesignerClient::~QDesignerClient()
{
    m_socket->close();
    m_socket->flush();
}

void QDesignerClient::readFromSocket()
{
    while (m_socket->canReadLine()) {
        QString file = QString::fromUtf8(m_socket->readLine());
        if (!file.isNull()) {
            file.remove(QLatin1Char('\n'));
            file.remove(QLatin1Char('\r'));
            if (QFile::exists(file))
                qDesigner->postEvent(qDesigner, new QFileOpenEvent(file));
        }
    }
}

QT_END_NAMESPACE
