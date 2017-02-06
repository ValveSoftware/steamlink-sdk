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

#include "testhttpserver.h"
#include <QTcpSocket>
#include <QDebug>
#include <QFile>
#include <QTimer>
#include <QTest>

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
TestHTTPServer server;
QVERIFY2(server.listen(14445), qPrintable(server.errorString()));
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

static QUrl localHostUrl(quint16 port)
{
    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(QStringLiteral("127.0.0.1"));
    url.setPort(port);
    return url;
}

TestHTTPServer::TestHTTPServer()
    : m_state(AwaitingHeader)
{
    QObject::connect(&m_server, &QTcpServer::newConnection, this, &TestHTTPServer::newConnection);
}

bool TestHTTPServer::listen()
{
    return m_server.listen(QHostAddress::LocalHost, 0);
}

QUrl TestHTTPServer::baseUrl() const
{
    return localHostUrl(m_server.serverPort());
}

quint16 TestHTTPServer::port() const
{
    return m_server.serverPort();
}

QUrl TestHTTPServer::url(const QString &documentPath) const
{
    return baseUrl().resolved(documentPath);
}

QString TestHTTPServer::urlString(const QString &documentPath) const
{
    return url(documentPath).toString();
}

QString TestHTTPServer::errorString() const
{
    return m_server.errorString();
}

bool TestHTTPServer::serveDirectory(const QString &dir, Mode mode)
{
    m_directories.append(qMakePair(dir, mode));
    return true;
}

/*
   Add an alias, so that if filename is requested and does not exist,
   alias may be returned.
*/
void TestHTTPServer::addAlias(const QString &filename, const QString &alias)
{
    m_aliases.insert(filename, alias);
}

void TestHTTPServer::addRedirect(const QString &filename, const QString &redirectName)
{
    m_redirects.insert(filename, redirectName);
}

void TestHTTPServer::registerFileNameForContentSubstitution(const QString &fileName)
{
    m_contentSubstitutedFileNames.insert(fileName);
}

bool TestHTTPServer::wait(const QUrl &expect, const QUrl &reply, const QUrl &body)
{
    m_state = AwaitingHeader;
    m_data.clear();

    QFile expectFile(expect.toLocalFile());
    if (!expectFile.open(QIODevice::ReadOnly))
        return false;

    QFile replyFile(reply.toLocalFile());
    if (!replyFile.open(QIODevice::ReadOnly))
        return false;

    m_bodyData = QByteArray();
    if (body.isValid()) {
        QFile bodyFile(body.toLocalFile());
        if (!bodyFile.open(QIODevice::ReadOnly))
            return false;
        m_bodyData = bodyFile.readAll();
    }

    const QByteArray serverHostUrl
        = QByteArrayLiteral("127.0.0.1:")+ QByteArray::number(m_server.serverPort());

    QByteArray line;
    bool headers_done = false;
    while (!(line = expectFile.readLine()).isEmpty()) {
        line.replace('\r', "");
        if (line.at(0) == '\n') {
            headers_done = true;
            continue;
        }
        if (headers_done) {
            m_waitData.body.append(line);
        } else {
            line.replace("{{ServerHostUrl}}", serverHostUrl);
            m_waitData.headers.append(line);
        }
    }
    /*
    while (waitData.endsWith('\n'))
        waitData = waitData.left(waitData.count() - 1);
        */

    m_replyData = replyFile.readAll();

    if (!m_replyData.endsWith('\n'))
        m_replyData.append('\n');
    m_replyData.append("Content-length: ");
    m_replyData.append(QByteArray::number(m_bodyData.length()));
    m_replyData.append("\n\n");

    for (int ii = 0; ii < m_replyData.count(); ++ii) {
        if (m_replyData.at(ii) == '\n' && (!ii || m_replyData.at(ii - 1) != '\r')) {
            m_replyData.insert(ii, '\r');
            ++ii;
        }
    }
    m_replyData.append(m_bodyData);

    return true;
}

bool TestHTTPServer::hasFailed() const
{
    return m_state == Failed;
}

void TestHTTPServer::newConnection()
{
    QTcpSocket *socket = m_server.nextPendingConnection();
    if (!socket)
        return;

    if (!m_directories.isEmpty())
        m_dataCache.insert(socket, QByteArray());

    QObject::connect(socket, &QAbstractSocket::disconnected, this, &TestHTTPServer::disconnected);
    QObject::connect(socket, &QIODevice::readyRead, this, &TestHTTPServer::readyRead);
}

void TestHTTPServer::disconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    m_dataCache.remove(socket);
    for (int ii = 0; ii < m_toSend.count(); ++ii) {
        if (m_toSend.at(ii).first == socket) {
            m_toSend.removeAt(ii);
            --ii;
        }
    }
    socket->disconnect();
    socket->deleteLater();
}

void TestHTTPServer::readyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket || socket->state() == QTcpSocket::ClosingState)
        return;

    if (!m_directories.isEmpty()) {
        serveGET(socket, socket->readAll());
        return;
    }

    if (m_state == Failed || (m_waitData.body.isEmpty() && m_waitData.headers.count() == 0)) {
        qWarning() << "TestHTTPServer: Unexpected data" << socket->readAll();
        return;
    }

    if (m_state == AwaitingHeader) {
        QByteArray line;
        while (!(line = socket->readLine()).isEmpty()) {
            line.replace('\r', "");
            if (line.at(0) == '\n') {
                m_state = AwaitingData;
                m_data += socket->readAll();
                break;
            } else {
                if (!m_waitData.headers.contains(line)) {
                    qWarning() << "TestHTTPServer: Unexpected header:" << line
                        << "\nExpected headers: " << m_waitData.headers;
                    m_state = Failed;
                    socket->disconnectFromHost();
                    return;
                }
            }
        }
    }  else {
        m_data += socket->readAll();
    }

    if (!m_data.isEmpty() || m_waitData.body.isEmpty()) {
        if (m_waitData.body != m_data) {
            qWarning() << "TestHTTPServer: Unexpected data" << m_data << "\nExpected: " << m_waitData.body;
            m_state = Failed;
        } else {
            socket->write(m_replyData);
        }
        socket->disconnectFromHost();
    }
}

bool TestHTTPServer::reply(QTcpSocket *socket, const QByteArray &fileNameIn)
{
    const QString fileName = QLatin1String(fileNameIn);
    if (m_redirects.contains(fileName)) {
        const QByteArray response
            = "HTTP/1.1 302 Found\r\nContent-length: 0\r\nContent-type: text/html; charset=UTF-8\r\nLocation: "
              + m_redirects.value(fileName).toUtf8() + "\r\n\r\n";
        socket->write(response);
        return true;
    }

    for (int ii = 0; ii < m_directories.count(); ++ii) {
        const QString &dir = m_directories.at(ii).first;
        const Mode mode = m_directories.at(ii).second;

        QString dirFile = dir + QLatin1Char('/') + fileName;

        if (!QFile::exists(dirFile)) {
            const QHash<QString, QString>::const_iterator it = m_aliases.constFind(fileName);
            if (it != m_aliases.constEnd())
                dirFile = dir + QLatin1Char('/') + it.value();
        }

        QFile file(dirFile);
        if (file.open(QIODevice::ReadOnly)) {

            if (mode == Disconnect)
                return true;

            QByteArray data = file.readAll();
            if (m_contentSubstitutedFileNames.contains(QLatin1Char('/') + fileName))
                data.replace(QByteArrayLiteral("{{ServerBaseUrl}}"), baseUrl().toString().toUtf8());

            QByteArray response
                = "HTTP/1.0 200 OK\r\nContent-type: text/html; charset=UTF-8\r\nContent-length: ";
            response += QByteArray::number(data.count());
            response += "\r\n\r\n";
            response += data;

            if (mode == Delay) {
                m_toSend.append(qMakePair(socket, response));
                QTimer::singleShot(500, this, &TestHTTPServer::sendOne);
                return false;
            } else {
                socket->write(response);
                return true;
            }
        }
    }

    socket->write("HTTP/1.0 404 Not found\r\nContent-type: text/html; charset=UTF-8\r\n\r\n");

    return true;
}

void TestHTTPServer::sendDelayedItem()
{
    sendOne();
}

void TestHTTPServer::sendOne()
{
    if (!m_toSend.isEmpty()) {
        m_toSend.first().first->write(m_toSend.first().second);
        m_toSend.first().first->close();
        m_toSend.removeFirst();
    }
}

void TestHTTPServer::serveGET(QTcpSocket *socket, const QByteArray &data)
{
    const QHash<QTcpSocket *, QByteArray>::iterator it = m_dataCache.find(socket);
    if (it == m_dataCache.end())
        return;

    QByteArray &total = it.value();
    total.append(data);

    if (total.contains("\n\r\n")) {
        bool close = true;
        if (total.startsWith("GET /")) {
            const int space = total.indexOf(' ', 4);
            if (space != -1)
                close = reply(socket, total.mid(5, space - 5));
        }
        m_dataCache.erase(it);
        if (close)
            socket->disconnectFromHost();
    }
}

ThreadedTestHTTPServer::ThreadedTestHTTPServer(const QString &dir, TestHTTPServer::Mode mode) :
    m_port(0)
{
    m_dirs[dir] = mode;
    start();
}

ThreadedTestHTTPServer::ThreadedTestHTTPServer(const QHash<QString, TestHTTPServer::Mode> &dirs) :
    m_dirs(dirs), m_port(0)
{
    start();
}

ThreadedTestHTTPServer::~ThreadedTestHTTPServer()
{
    quit();
    wait();
}

QUrl ThreadedTestHTTPServer::baseUrl() const
{
    return localHostUrl(m_port);
}

QUrl ThreadedTestHTTPServer::url(const QString &documentPath) const
{
    return baseUrl().resolved(documentPath);
}

QString ThreadedTestHTTPServer::urlString(const QString &documentPath) const
{
    return url(documentPath).toString();
}

void ThreadedTestHTTPServer::run()
{
    TestHTTPServer server;
    {
        QMutexLocker locker(&m_mutex);
        QVERIFY2(server.listen(), qPrintable(server.errorString()));
        m_port = server.port();
        for (QHash<QString, TestHTTPServer::Mode>::ConstIterator i = m_dirs.constBegin();
             i != m_dirs.constEnd(); ++i) {
            server.serveDirectory(i.key(), i.value());
        }
        m_condition.wakeAll();
    }
    exec();
}

void ThreadedTestHTTPServer::start()
{
    QMutexLocker locker(&m_mutex);
    QThread::start();
    m_condition.wait(&m_mutex);
}
