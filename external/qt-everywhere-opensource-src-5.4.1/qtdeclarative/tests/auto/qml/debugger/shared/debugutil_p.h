
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

#ifndef DEBUGUTIL_H
#define DEBUGUTIL_H

#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QTest>
#include <QProcess>
#include <QMutex>

#include <QtQml/qqmlengine.h>

#include "qqmldebugclient.h"

class QQmlDebugTest
{
public:
    static bool waitForSignal(QObject *receiver, const char *member, int timeout = 5000);
};

class QQmlDebugTestClient : public QQmlDebugClient
{
    Q_OBJECT
public:
    QQmlDebugTestClient(const QString &s, QQmlDebugConnection *c);

    QByteArray waitForResponse();

signals:
    void stateHasChanged();
    void serverMessage(const QByteArray &);

protected:
    virtual void stateChanged(State state);
    virtual void messageReceived(const QByteArray &ba);

private:
    QByteArray lastMsg;
};

class QQmlDebugProcess : public QObject
{
    Q_OBJECT
public:
    QQmlDebugProcess(const QString &executable, QObject *parent = 0);
    ~QQmlDebugProcess();

    QString state();

    void setEnvironment(const QStringList &environment);

    void start(const QStringList &arguments);
    bool waitForSessionStart();
    int debugPort() const;

    bool waitForFinished();
    QProcess::ExitStatus exitStatus() const;

    QString output() const;
    void stop();

signals:
    void readyReadStandardOutput();

private slots:
    void timeout();
    void processAppOutput();
    void processError(QProcess::ProcessError error);

private:
    QString m_executable;
    QProcess m_process;
    QString m_outputBuffer;
    QString m_output;
    QTimer m_timer;
    QEventLoop m_eventLoop;
    QMutex m_mutex;
    bool m_started;
    QStringList m_environment;
    int m_port;
};

#endif // DEBUGUTIL_H
