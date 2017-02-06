/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QMLPROFILERAPPLICATION_H
#define QMLPROFILERAPPLICATION_H

#include "qmlprofilerclient.h"
#include "qmlprofilerdata.h"

#include <private/qqmldebugconnection_p.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qprocess.h>
#include <QtCore/qtimer.h>
#include <QtNetwork/qabstractsocket.h>

enum PendingRequest {
    REQUEST_QUIT,
    REQUEST_FLUSH_FILE,
    REQUEST_FLUSH,
    REQUEST_OUTPUT_FILE,
    REQUEST_TOGGLE_RECORDING,
    REQUEST_NONE
};

class QmlProfilerApplication : public QCoreApplication
{
    Q_OBJECT
public:
    QmlProfilerApplication(int &argc, char **argv);
    ~QmlProfilerApplication();

    void parseArguments();
    int exec();
    bool isInteractive() const;
    void userCommand(const QString &command);
    void notifyTraceStarted();
    void outputData();

signals:
    void readyForCommand();

private:
    void run();
    void tryToConnect();
    void connected();
    void processHasOutput();
    void processFinished();

    void traceClientEnabledChanged(bool enabled);
    void traceFinished();

    void prompt(const QString &line = QString(), bool ready = true);
    void logError(const QString &error);
    void logStatus(const QString &status);

    quint64 parseFeatures(const QStringList &featureList, const QString &values, bool exclude);
    bool checkOutputFile(PendingRequest pending);
    void flush();
    void output();

    enum ApplicationMode {
        LaunchMode,
        AttachMode
    } m_runMode;

    // LaunchMode
    QString m_programPath;
    QStringList m_programArguments;
    QProcess *m_process;

    QString m_socketFile;
    QString m_hostName;
    quint16 m_port;
    QString m_outputFile;
    QString m_interactiveOutputFile;

    PendingRequest m_pendingRequest;
    bool m_verbose;
    bool m_recording;
    bool m_interactive;

    QQmlDebugConnection m_connection;
    QmlProfilerClient m_qmlProfilerClient;
    QmlProfilerData m_profilerData;
    QTimer m_connectTimer;
    uint m_connectionAttempts;
};

#endif // QMLPROFILERAPPLICATION_H
