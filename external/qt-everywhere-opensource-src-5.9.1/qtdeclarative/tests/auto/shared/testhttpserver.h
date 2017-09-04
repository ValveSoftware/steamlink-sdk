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

#ifndef TESTHTTPSERVER_H
#define TESTHTTPSERVER_H

#include <QTcpServer>
#include <QUrl>
#include <QPair>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>

class TestHTTPServer : public QObject
{
    Q_OBJECT
public:
    TestHTTPServer();

    bool listen();
    quint16 port() const;
    QUrl baseUrl() const;
    QUrl url(const QString &documentPath) const;
    QString urlString(const QString &documentPath) const;
    QString errorString() const;

    enum Mode { Normal, Delay, Disconnect };
    bool serveDirectory(const QString &, Mode = Normal);

    bool wait(const QUrl &expect, const QUrl &reply, const QUrl &body);
    bool hasFailed() const;

    void addAlias(const QString &filename, const QString &aliasName);
    void addRedirect(const QString &filename, const QString &redirectName);

    void registerFileNameForContentSubstitution(const QString &fileName);

    // In Delay mode, each item needs one call to this function to be sent
    void sendDelayedItem();

private slots:
    void newConnection();
    void disconnected();
    void readyRead();
    void sendOne();

private:
    enum State {
        AwaitingHeader,
        AwaitingData,
        Failed
    };

    void serveGET(QTcpSocket *, const QByteArray &);
    bool reply(QTcpSocket *, const QByteArray &);

    QList<QPair<QString, Mode> > m_directories;
    QHash<QTcpSocket *, QByteArray> m_dataCache;
    QList<QPair<QTcpSocket *, QByteArray> > m_toSend;
    QSet<QString> m_contentSubstitutedFileNames;

    struct WaitData {
        QList <QByteArray>headers;
        QByteArray body;
    } m_waitData;
    QByteArray m_replyData;
    QByteArray m_bodyData;
    QByteArray m_data;
    State m_state;

    QHash<QString, QString> m_aliases;
    QHash<QString, QString> m_redirects;

    QTcpServer m_server;
};

class ThreadedTestHTTPServer : public QThread
{
    Q_OBJECT
public:
    ThreadedTestHTTPServer(const QString &dir, TestHTTPServer::Mode mode = TestHTTPServer::Normal);
    ThreadedTestHTTPServer(const QHash<QString, TestHTTPServer::Mode> &dirs);
    ~ThreadedTestHTTPServer();

    QUrl baseUrl() const;
    QUrl url(const QString &documentPath) const;
    QString urlString(const QString &documentPath) const;

protected:
    void run();

private:
    void start();

    QHash<QString, TestHTTPServer::Mode> m_dirs;
    quint16 m_port;
    QMutex m_mutex;
    QWaitCondition m_condition;
};

#endif // TESTHTTPSERVER_H

