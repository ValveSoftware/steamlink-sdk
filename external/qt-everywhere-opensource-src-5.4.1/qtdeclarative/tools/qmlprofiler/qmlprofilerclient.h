/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QMLPROFILERCLIENT_H
#define QMLPROFILERCLIENT_H

#include "qqmldebugclient.h"
#include <QtQml/private/qqmlprofilerservice_p.h>
#include "qmlprofilereventlocation.h"

class ProfilerClientPrivate;
class ProfilerClient : public QQmlDebugClient
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ isEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool recording READ isRecording WRITE setRecording
               NOTIFY recordingChanged)

public:
    ProfilerClient(const QString &clientName,
                  QQmlDebugConnection *client);
    ~ProfilerClient();

    bool isEnabled() const;
    bool isRecording() const;

public slots:
    void setRecording(bool);
    virtual void clearData();
    virtual void sendRecordingStatus();

signals:
    void complete();
    void recordingChanged(bool arg);
    void enabledChanged();
    void cleared();

protected:
    virtual void stateChanged(State);

protected:
    bool m_recording;
    bool m_enabled;
};

class QmlProfilerClient : public ProfilerClient
{
    Q_OBJECT

public:
    QmlProfilerClient(QQmlDebugConnection *client);
    ~QmlProfilerClient();

public slots:
    void clearData();
    void sendRecordingStatus();

signals:
    void traceFinished( qint64 time );
    void traceStarted( qint64 time );
    void range(QQmlProfilerService::RangeType type,
               QQmlProfilerService::BindingType bindingType,
               qint64 startTime, qint64 length,
               const QStringList &data,
               const QmlEventLocation &location);
    void frame(qint64 time, int frameRate, int animationCount, int threadId);
    void sceneGraphFrame(QQmlProfilerService::SceneGraphFrameType type, qint64 time,
                         qint64 numericData1, qint64 numericData2, qint64 numericData3,
                         qint64 numericData4, qint64 numericData5);
    void pixmapCache(QQmlProfilerService::PixmapEventType, qint64 time,
                     const QmlEventLocation &location, int width, int height, int refCount);
    void memoryAllocation(QQmlProfilerService::MemoryType type, qint64 time, qint64 amount);

protected:
    virtual void messageReceived(const QByteArray &);

private:
    class QmlProfilerClientPrivate *d;
};

class V8ProfilerClient : public ProfilerClient
{
    Q_OBJECT

public:
    enum Message {
        V8Entry,
        V8Complete,

        V8MaximumMessage
    };

    V8ProfilerClient(QQmlDebugConnection *client);
    ~V8ProfilerClient();

public slots:
    void sendRecordingStatus();

signals:
    void range(int depth, const QString &function, const QString &filename,
               int lineNumber, double totalTime, double selfTime);

protected:
    virtual void messageReceived(const QByteArray &);
};

#endif // QMLPROFILERCLIENT_H
