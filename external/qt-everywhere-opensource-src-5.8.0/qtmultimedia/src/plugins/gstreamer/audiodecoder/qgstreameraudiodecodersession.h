/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QGSTREAMERPLAYERSESSION_H
#define QGSTREAMERPLAYERSESSION_H

#include <QObject>
#include <QtCore/qmutex.h>
#include "qgstreameraudiodecodercontrol.h"
#include <private/qgstreamerbushelper_p.h>
#include "qaudiodecoder.h"

#if defined(HAVE_GST_APPSRC)
#include <private/qgstappsrc_p.h>
#endif

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

QT_BEGIN_NAMESPACE

class QGstreamerBusHelper;
class QGstreamerMessage;

class QGstreamerAudioDecoderSession : public QObject,
                                public QGstreamerBusMessageFilter
{
Q_OBJECT
Q_INTERFACES(QGstreamerBusMessageFilter)

public:
    QGstreamerAudioDecoderSession(QObject *parent);
    virtual ~QGstreamerAudioDecoderSession();

    QGstreamerBusHelper *bus() const { return m_busHelper; }

    QAudioDecoder::State state() const { return m_state; }
    QAudioDecoder::State pendingState() const { return m_pendingState; }

    bool processBusMessage(const QGstreamerMessage &message);

#if defined(HAVE_GST_APPSRC)
    QGstAppSrc *appsrc() const { return m_appSrc; }
    static void configureAppSrcElement(GObject*, GObject*, GParamSpec*,QGstreamerAudioDecoderSession* _this);
#endif

    QString sourceFilename() const;
    void setSourceFilename(const QString &fileName);

    QIODevice* sourceDevice() const;
    void setSourceDevice(QIODevice *device);

    void start();
    void stop();

    QAudioFormat audioFormat() const;
    void setAudioFormat(const QAudioFormat &format);

    QAudioBuffer read();
    bool bufferAvailable() const;

    qint64 position() const;
    qint64 duration() const;

    static GstFlowReturn new_sample(GstAppSink *sink, gpointer user_data);

signals:
    void stateChanged(QAudioDecoder::State newState);
    void formatChanged(const QAudioFormat &format);
    void sourceChanged();

    void error(int error, const QString &errorString);

    void bufferReady();
    void bufferAvailableChanged(bool available);
    void finished();

    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);

private slots:
    void updateDuration();

private:
    void setAudioFlags(bool wantNativeAudio);
    void addAppSink();
    void removeAppSink();

    void processInvalidMedia(QAudioDecoder::Error errorCode, const QString& errorString);
    static qint64 getPositionFromBuffer(GstBuffer* buffer);

    QAudioDecoder::State m_state;
    QAudioDecoder::State m_pendingState;
    QGstreamerBusHelper *m_busHelper;
    GstBus *m_bus;
    GstElement *m_playbin;
    GstElement *m_outputBin;
    GstElement *m_audioConvert;
    GstAppSink *m_appSink;

#if defined(HAVE_GST_APPSRC)
    QGstAppSrc *m_appSrc;
#endif

    QString mSource;
    QIODevice *mDevice; // QWeakPointer perhaps
    QAudioFormat mFormat;

    mutable QMutex m_buffersMutex;
    int m_buffersAvailable;

    qint64 m_position;
    qint64 m_duration;

    int m_durationQueries;
};

QT_END_NAMESPACE

#endif // QGSTREAMERPLAYERSESSION_H
