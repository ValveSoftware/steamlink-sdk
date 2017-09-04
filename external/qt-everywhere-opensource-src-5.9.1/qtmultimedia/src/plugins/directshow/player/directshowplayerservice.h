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

#ifndef DIRECTSHOWPLAYERSERVICE_H
#define DIRECTSHOWPLAYERSERVICE_H

#include <dshow.h>

#include "qmediaplayer.h"
#include "qmediaresource.h"
#include "qmediaservice.h"
#include "qmediatimerange.h"

#include "directshoweventloop.h"
#include "directshowglobal.h"

#include <QtCore/qcoreevent.h>
#include <QtCore/qmutex.h>
#include <QtCore/qurl.h>
#include <QtCore/qwaitcondition.h>

QT_BEGIN_NAMESPACE

class DirectShowAudioEndpointControl;
class DirectShowMetaDataControl;
class DirectShowPlayerControl;
class DirectShowVideoRendererControl;
class DirectShowAudioProbeControl;
class DirectShowVideoProbeControl;
class DirectShowSampleGrabber;

class QMediaContent;
class QVideoWindowControl;

class DirectShowPlayerService : public QMediaService
{
    Q_OBJECT
public:
    enum StreamType
    {
        AudioStream = 0x01,
        VideoStream = 0x02
    };

    DirectShowPlayerService(QObject *parent = 0);
    ~DirectShowPlayerService();

    QMediaControl* requestControl(const char *name);
    void releaseControl(QMediaControl *control);

    void load(const QMediaContent &media, QIODevice *stream);
    void play();
    void pause();
    void stop();

    qint64 position() const;
    QMediaTimeRange availablePlaybackRanges() const;

    void seek(qint64 position);
    void setRate(qreal rate);

    int bufferStatus() const;

    void setAudioOutput(IBaseFilter *filter);
    void setVideoOutput(IBaseFilter *filter);

protected:
    void customEvent(QEvent *event);

private Q_SLOTS:
    void videoOutputChanged();

    void onAudioBufferAvailable(double time, quint8 *buffer, long len);
    void onVideoBufferAvailable(double time, quint8 *buffer, long len);

private:
    void releaseGraph();
    void updateStatus();

    void updateAudioProbe();
    void updateVideoProbe();

    int findStreamTypes(IBaseFilter *source) const;
    int findStreamType(IPin *pin) const;

    bool isConnected(IBaseFilter *filter, PIN_DIRECTION direction) const;
    IBaseFilter *getConnected(IBaseFilter *filter, PIN_DIRECTION direction) const;

    void run();

    void doSetUrlSource(QMutexLocker *locker);
    void doSetStreamSource(QMutexLocker *locker);
    void doRender(QMutexLocker *locker);
    void doFinalizeLoad(QMutexLocker *locker);
    void doSetRate(QMutexLocker *locker);
    void doSeek(QMutexLocker *locker);
    void doPlay(QMutexLocker *locker);
    void doPause(QMutexLocker *locker);
    void doStop(QMutexLocker *locker);
    void doReleaseAudioOutput(QMutexLocker *locker);
    void doReleaseVideoOutput(QMutexLocker *locker);
    void doReleaseGraph(QMutexLocker *locker);
    void doSetVideoProbe(QMutexLocker *locker);
    void doSetAudioProbe(QMutexLocker *locker);
    void doReleaseVideoProbe(QMutexLocker *locker);
    void doReleaseAudioProbe(QMutexLocker *locker);

    void graphEvent(QMutexLocker *locker);

    enum Task
    {
        Shutdown           = 0x00001,
        SetUrlSource       = 0x00002,
        SetStreamSource    = 0x00004,
        SetSource          = SetUrlSource | SetStreamSource,
        SetAudioOutput     = 0x00008,
        SetVideoOutput     = 0x00010,
        SetOutputs         = SetAudioOutput | SetVideoOutput,
        SetAudioProbe      = 0x00020,
        SetVideoProbe      = 0x00040,
        SetProbes          = SetAudioProbe | SetVideoProbe,
        Render             = 0x00080,
        FinalizeLoad       = 0x00100,
        SetRate            = 0x00200,
        Seek               = 0x00400,
        Play               = 0x00800,
        Pause              = 0x01000,
        Stop               = 0x02000,
        ReleaseGraph       = 0x04000,
        ReleaseAudioOutput = 0x08000,
        ReleaseVideoOutput = 0x10000,
        ReleaseAudioProbe  = 0x20000,
        ReleaseVideoProbe  = 0x40000,
        ReleaseFilters     = ReleaseGraph | ReleaseAudioOutput
                             | ReleaseVideoOutput | ReleaseAudioProbe
                             | ReleaseVideoProbe
    };

    enum Event
    {
        FinalizedLoad = QEvent::User,
        Error,
        RateChange,
        Started,
        Paused,
        DurationChange,
        StatusChange,
        EndOfMedia,
        PositionChange
    };

    enum GraphStatus
    {
        NoMedia,
        Loading,
        Loaded,
        InvalidMedia
    };

    DirectShowPlayerControl *m_playerControl;
    DirectShowMetaDataControl *m_metaDataControl;
    DirectShowVideoRendererControl *m_videoRendererControl;
    QVideoWindowControl *m_videoWindowControl;
    DirectShowAudioEndpointControl *m_audioEndpointControl;
    DirectShowAudioProbeControl *m_audioProbeControl;
    DirectShowVideoProbeControl *m_videoProbeControl;
    DirectShowSampleGrabber *m_audioSampleGrabber;
    DirectShowSampleGrabber *m_videoSampleGrabber;

    QThread *m_taskThread;
    DirectShowEventLoop *m_loop;
    int m_pendingTasks;
    int m_executingTask;
    int m_executedTasks;
    int m_streamTypes;
    HANDLE m_taskHandle;
    HANDLE m_eventHandle;
    GraphStatus m_graphStatus;
    QMediaPlayer::Error m_error;
    QIODevice *m_stream;
    IFilterGraph2 *m_graph;
    IBaseFilter *m_source;
    IBaseFilter *m_audioOutput;
    IBaseFilter *m_videoOutput;
    qreal m_rate;
    qint64 m_position;
    qint64 m_seekPosition;
    qint64 m_duration;
    QMediaTimeRange m_playbackRange;
    QUrl m_url;
    QMediaResourceList m_resources;
    QString m_errorString;
    QMutex m_mutex;
    bool m_buffering;
    bool m_seekable;
    bool m_atEnd;
    bool m_dontCacheNextSeekResult;

    friend class DirectShowPlayerServiceThread;
};

QT_END_NAMESPACE

#endif
