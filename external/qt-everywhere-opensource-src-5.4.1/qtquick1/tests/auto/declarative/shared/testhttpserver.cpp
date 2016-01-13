/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "testhttpserver.h"
#include <QTcpSocket>
#include <QDebug>
#include <QFile>
#include <QTimer>

/*!
\internal
\class TestHTTPServer
\brief provides a very, very basic HTTP server for testing.

Inside the test case, an instance of TestHTTPServer should be created, with the
appropriate port to listen on.  The server will listen on the localhost interface.

Directories to serve can then be added to server, which will be added as "roots".
Each root can be added as a Normal, Delay or Disconnect root.  Requests for files
within a Normal root are returned immediately.  Request for files within a Delay
root are delayed for 500ms, and then served.  Requests for files within a Disconnect
directory cause the server to disconnect immediately.  A request for a file that isn't
found in any root will return a 404 error.

If you have the following directory structure:

\code
disconnect/disconnectTest.qml
files/main.qml
files/Button.qml
files/content/WebView.qml
slowFiles/slowMain.qml
\endcode
it can be added like this:
\code
TestHTTPServer server(14445);
server.serveDirectory("disconnect", TestHTTPServer::Disconnect);
server.serveDirectory("files");
server.serveDirectory("slowFiles", TestHTTPServer::Delay);
\endcode

The following request urls will then result in the appropriate action:
\table
\header \li URL \li Action
\row \li http://localhost:14445/disconnectTest.qml \li Disconnection
\row \li http://localhost:14445/main.qml \li main.qml returned immediately
\row \li http://localhost:14445/Button.qml \li Button.qml returned immediately
\row \li http://localhost:14445/content/WebView.qml \li content/WebView.qml returned immediately
\row \li http://localhost:14445/slowMain.qml \li slowMain.qml returned after 500ms
\endtable
*/
TestHTTPServer::TestHTTPServer(quint16 port)
: m_hasFailed(false)
{
    QObject::connect(&server, SIGNAL(newConnection()), this, SLOT(newConnection()));

    server.listen(QHostAddress::LocalHost, port);
}

bool TestHTTPServer::isValid() const
{
    return server.isListening();
}

bool TestHTTPServer::serveDirectory(const QString &dir, Mode mode)
{
    dirs.append(qMakePair(dir, mode));
    return true;
}

/*
   Add an alias, so that if filename is requested and does not exist,
   alias may be returned.
*/
void TestHTTPServer::addAlias(const QString &filename, const QString &alias)
{
    aliases.insert(filename, alias);
}

void TestHTTPServer::addRedirect(const QString &filename, const QString &redirectName)
{
    redirects.insert(filename, redirectName);
}

bool TestHTTPServer::wait(const QUrl &expect, const QUrl &reply, const QUrl &body)
{
    m_hasFailed = false;

    QFile expectFile(expect.toLocalFile());
    if (!expectFile.open(QIODevice::ReadOnly)) return false;

    QFile replyFile(reply.toLocalFile());
    if (!replyFile.open(QIODevice::ReadOnly)) return false;

    bodyData = QByteArray();
    if (body.isValid()) {
        QFile bodyFile(body.toLocalFile());
        if (!bodyFile.open(QIODevice::ReadOnly)) return false;
        bodyData = bodyFile.readAll();
    }

    waitData = expectFile.readAll();
    /*
    while (waitData.endsWith('\n'))
        waitData = waitData.left(waitData.count() - 1);
        */

    replyData = replyFile.readAll();

    if (!replyData.endsWith('\n'))
        replyData.append("\n");
    replyData.append("Content-length: " + QByteArray::number(bodyData.length()));
    replyData .append("\n\n");

    for (int ii = 0; ii < replyData.count(); ++ii) {
        if (replyData.at(ii) == '\n' && (!ii || replyData.at(ii - 1) != '\r')) {
            replyData.insert(ii, '\r');
            ++ii;
        }
    }
    replyData.append(bodyData);

    return true;
}

bool TestHTTPServer::hasFailed() const
{
    return m_hasFailed;
}

void TestHTTPServer::newConnection()
{
    QTcpSocket *socket = server.nextPendingConnection();
    if (!socket) return;

    if (!dirs.isEmpty())
        dataCache.insert(socket, QByteArray());

    QObject::connect(socket, SIGNAL(disconnected()), this, SLOT(disconnected()));
    QObject::connect(socket, SIGNAL(readyRead()), this, SLOT(readyRead()));
}

void TestHTTPServer::disconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) return;

    dataCache.remove(socket);
    for (int ii = 0; ii < toSend.count(); ++ii) {
        if (toSend.at(ii).first == socket) {
            toSend.removeAt(ii);
            --ii;
        }
    }
    socket->disconnect();
    socket->deleteLater();
}

void TestHTTPServer::readyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket || socket->state() == QTcpSocket::ClosingState) return;

    QByteArray ba = socket->readAll();

    if (!dirs.isEmpty()) {
        serveGET(socket, ba);
        return;
    }

    if (m_hasFailed || waitData.isEmpty()) {
        qWarning() << "TestHTTPServer: Unexpected data" << ba;
        return;
    }

    for (int ii = 0; ii < ba.count(); ++ii) {
        const char c = ba.at(ii);
        if (c == '\r' && waitData.isEmpty())
           continue;
        else if (!waitData.isEmpty() && c == waitData.at(0))
            waitData = waitData.mid(1);
        else if (c == '\r')
            continue;
        else {
            QByteArray data = ba.mid(ii);
            qWarning() << "TestHTTPServer: Unexpected data" << data << "\nExpected: " << waitData;
            m_hasFailed = true;
            socket->disconnectFromHost();
            return;
        }
    }

    if (waitData.isEmpty()) {
        socket->write(replyData);
        socket->disconnectFromHost();
    }
}

bool TestHTTPServer::reply(QTcpSocket *socket, const QByteArray &fileName)
{
    if (redirects.contains(fileName)) {
        QByteArray response = "HTTP/1.1 302 Found\r\nContent-length: 0\r\nContent-type: text/html; charset=UTF-8\r\nLocation: " + redirects[fileName].toUtf8() + "\r\n\r\n";
        socket->write(response);
        return true;
    }

    for (int ii = 0; ii < dirs.count(); ++ii) {
        QString dir = dirs.at(ii).first;
        Mode mode = dirs.at(ii).second;

        QString dirFile = dir + QLatin1String("/") + QLatin1String(fileName);

        if (!QFile::exists(dirFile)) {
            if (aliases.contains(fileName))
                dirFile = dir + QLatin1String("/") + aliases.value(fileName);
        }

        QFile file(dirFile);
        if (file.open(QIODevice::ReadOnly)) {

            if (mode == Disconnect)
                return true;

            QByteArray data = file.readAll();

            QByteArray response = "HTTP/1.0 200 OK\r\nContent-type: text/html; charset=UTF-8\r\nContent-length: ";
            response += QByteArray::number(data.count());
            response += "\r\n\r\n";
            response += data;

            if (mode == Delay) {
                toSend.append(qMakePair(socket, response));
                QTimer::singleShot(500, this, SLOT(sendOne()));
                return false;
            } else {
                socket->write(response);
                return true;
            }
        }
    }


    QByteArray response = "HTTP/1.0 404 Not found\r\nContent-type: text/html; charset=UTF-8\r\n\r\n";
    socket->write(response);

    return true;
}

void TestHTTPServer::sendOne()
{
    if (!toSend.isEmpty()) {
        toSend.first().first->write(toSend.first().second);
        toSend.first().first->close();
        toSend.removeFirst();
    }
}

void TestHTTPServer::serveGET(QTcpSocket *socket, const QByteArray &data)
{
    if (!dataCache.contains(socket))
        return;

    QByteArray total = dataCache[socket] + data;
    dataCache[socket] = total;

    if (total.contains("\n\r\n")) {

        bool close = true;

        if (total.startsWith("GET /")) {

            int space = total.indexOf(' ', 4);
            if (space != -1) {

                QByteArray req = total.mid(5, space - 5);
                close = reply(socket, req);

            }
        }
        dataCache.remove(socket);

        if (close)
            socket->disconnectFromHost();
    }
}

