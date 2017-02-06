
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

#ifndef DEBUGUTIL_P_H
#define DEBUGUTIL_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qqmldebugclient_p.h>

#include <QtCore/qeventloop.h>
#include <QtCore/qtimer.h>
#include <QtCore/qthread.h>
#include <QtCore/qprocess.h>
#include <QtCore/qmutex.h>
#include <QtTest/qtest.h>
#include <QtQml/qqmlengine.h>

class QQmlDebugTest
{
public:
    static bool waitForSignal(QObject *receiver, const char *member, int timeout = 5000);
    static QList<QQmlDebugClient *> createOtherClients(QQmlDebugConnection *connection);
    static QString clientStateString(const QQmlDebugClient *client);
    static QString connectionStateString(const QQmlDebugConnection *connection);
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

    void addEnvironment(const QString &environment);

    void start(const QStringList &arguments);
    bool waitForSessionStart();
    int debugPort() const;

    bool waitForFinished();
    QProcess::ExitStatus exitStatus() const;

    QString output() const;
    void stop();
    void setMaximumBindErrors(int numErrors);

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
    int m_maximumBindErrors;
    int m_receivedBindErrors;
};

class QQmlInspectorResultRecipient : public QObject
{
    Q_OBJECT
public:
    QQmlInspectorResultRecipient(QObject *parent = 0) :
        QObject(parent), lastResponseId(-1), lastResult(false) {}

    int lastResponseId;
    bool lastResult;

    void recordResponse(int requestId, bool result)
    {
        lastResponseId = requestId;
        lastResult = result;
    }
};

#endif // DEBUGUTIL_P_H
