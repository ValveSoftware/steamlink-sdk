/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Mobility Components.
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

#ifndef MFPLAYERSESSION_H
#define MFPLAYERSESSION_H

#include <mfapi.h>
#include <mfidl.h>

#include "qmediaplayer.h"
#include "qmediaresource.h"
#include "qmediaservice.h"
#include "qmediatimerange.h"

#include <QtCore/qcoreevent.h>
#include <QtCore/qmutex.h>
#include <QtCore/qurl.h>
#include <QtCore/qwaitcondition.h>
#include <QtMultimedia/qaudioformat.h>
#include <QtMultimedia/qvideosurfaceformat.h>

QT_BEGIN_NAMESPACE
class QMediaContent;
QT_END_NAMESPACE

QT_USE_NAMESPACE

class SourceResolver;
#ifndef Q_WS_SIMULATOR
class Evr9VideoWindowControl;
#endif
class MFAudioEndpointControl;
class MFVideoRendererControl;
class MFPlayerControl;
class MFMetaDataControl;
class MFPlayerService;
class AudioSampleGrabberCallback;
class MFTransform;
class MFAudioProbeControl;
class MFVideoProbeControl;

class MFPlayerSession : public QObject, public IMFAsyncCallback
{
    Q_OBJECT
    friend class SourceResolver;
public:
    MFPlayerSession(MFPlayerService *playerService = 0);
    ~MFPlayerSession();

    STDMETHODIMP QueryInterface(REFIID riid, LPVOID *ppvObject);

    STDMETHODIMP_(ULONG) AddRef(void);

    STDMETHODIMP_(ULONG) Release(void);

    STDMETHODIMP Invoke(IMFAsyncResult *pResult);

    STDMETHODIMP GetParameters(DWORD *pdwFlags, DWORD *pdwQueue)
    {
        Q_UNUSED(pdwFlags);
        Q_UNUSED(pdwQueue);
        return E_NOTIMPL;
    }

    void load(const QMediaContent &media, QIODevice *stream);
    void stop(bool immediate = false);
    void start();
    void pause();

    QMediaPlayer::MediaStatus status() const;
    qint64 position();
    void setPosition(qint64 position);
    qreal playbackRate() const;
    void setPlaybackRate(qreal rate);
    int volume() const;
    void setVolume(int volume);
    bool isMuted() const;
    void setMuted(bool muted);
    int bufferStatus();
    QMediaTimeRange availablePlaybackRanges();

    void changeStatus(QMediaPlayer::MediaStatus newStatus);

    void close();

    void addProbe(MFAudioProbeControl* probe);
    void removeProbe(MFAudioProbeControl* probe);
    void addProbe(MFVideoProbeControl* probe);
    void removeProbe(MFVideoProbeControl* probe);

Q_SIGNALS:
    void error(QMediaPlayer::Error error, QString errorString, bool isFatal);
    void sessionEvent(IMFMediaEvent  *sessionEvent);
    void statusChanged();
    void audioAvailable();
    void videoAvailable();
    void durationUpdate(qint64 duration);
    void seekableUpdate(bool seekable);
    void positionChanged(qint64 position);
    void playbackRateChanged(qreal rate);
    void volumeChanged(int volume);
    void mutedChanged(bool muted);
    void bufferStatusChanged(int percentFilled);

private Q_SLOTS:
    void handleMediaSourceReady();
    void handleSessionEvent(IMFMediaEvent *sessionEvent);
    void handleSourceError(long hr);

private:
    long m_cRef;
    MFPlayerService *m_playerService;
    IMFMediaSession *m_session;
    IMFPresentationClock *m_presentationClock;
    IMFRateControl *m_rateControl;
    IMFRateSupport *m_rateSupport;
    IMFAudioStreamVolume *m_volumeControl;
    IPropertyStore *m_netsourceStatistics;
    PROPVARIANT m_varStart;
    UINT64 m_duration;

    enum Command
    {
        CmdNone = 0,
        CmdStop,
        CmdStart,
        CmdPause,
        CmdSeek,
        CmdSeekResume,
        CmdStartAndSeek
    };

    void clear();
    void setPositionInternal(qint64 position, Command requestCmd);
    void setPlaybackRateInternal(qreal rate);
    void commitRateChange(qreal rate, BOOL isThin);
    bool canScrub() const;
    void scrub(bool enableScrub);
    bool m_scrubbing;
    float m_restoreRate;

    SourceResolver  *m_sourceResolver;
    HANDLE           m_hCloseEvent;
    bool m_closing;

    enum MediaType
    {
        Unknown = 0,
        Audio = 1,
        Video = 2,
    };
    DWORD m_mediaTypes;

    enum PendingState
    {
        NoPending = 0,
        CmdPending,
        SeekPending,
    };

    struct SeekState
    {
        void setCommand(Command cmd) {
            prevCmd = command;
            command = cmd;
        }
        Command command;
        Command prevCmd;
        float   rate;        // Playback rate
        BOOL    isThin;      // Thinned playback?
        qint64  start;       // Start position
    };
    SeekState   m_state;        // Current nominal state.
    SeekState   m_request;      // Pending request.
    PendingState m_pendingState;
    float m_pendingRate;
    void updatePendingCommands(Command command);

    QMediaPlayer::MediaStatus m_status;
    bool m_canScrub;
    int m_volume;
    bool m_muted;

    void setVolumeInternal(int volume);

    void createSession();
    void setupPlaybackTopology(IMFMediaSource *source, IMFPresentationDescriptor *sourcePD);
    IMFTopologyNode* addSourceNode(IMFTopology* topology, IMFMediaSource* source,
        IMFPresentationDescriptor* presentationDesc, IMFStreamDescriptor *streamDesc);
    IMFTopologyNode* addOutputNode(IMFStreamDescriptor *streamDesc, MediaType& mediaType, IMFTopology* topology, DWORD sinkID);

    bool addAudioSampleGrabberNode(IMFTopology* topology);
    bool setupAudioSampleGrabber(IMFTopology *topology, IMFTopologyNode *sourceNode, IMFTopologyNode *outputNode);
    QAudioFormat audioFormatForMFMediaType(IMFMediaType *mediaType) const;
    AudioSampleGrabberCallback *m_audioSampleGrabber;
    IMFTopologyNode *m_audioSampleGrabberNode;

    IMFTopology *insertMFT(IMFTopology *topology, TOPOID outputNodeId);
    bool insertResizer(IMFTopology *topology);
    void insertColorConverter(IMFTopology *topology, TOPOID outputNodeId);
    MFTransform *m_videoProbeMFT;
    QList<MFVideoProbeControl*> m_videoProbes;
};


#endif
