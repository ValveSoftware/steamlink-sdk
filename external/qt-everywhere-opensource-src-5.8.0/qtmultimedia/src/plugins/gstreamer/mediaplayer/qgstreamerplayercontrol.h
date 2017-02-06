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

#ifndef QGSTREAMERPLAYERCONTROL_H
#define QGSTREAMERPLAYERCONTROL_H

#include <QtCore/qobject.h>
#include <QtCore/qstack.h>

#include <qmediaplayercontrol.h>
#include <qmediaplayer.h>

#include <limits.h>

QT_BEGIN_NAMESPACE

class QMediaPlayerResourceSetInterface;

class QMediaPlaylist;
class QMediaPlaylistNavigator;
class QSocketNotifier;

class QGstreamerPlayerSession;
class QGstreamerPlayerService;

class QGstreamerPlayerControl : public QMediaPlayerControl
{
    Q_OBJECT

public:
    QGstreamerPlayerControl(QGstreamerPlayerSession *session, QObject *parent = 0);
    ~QGstreamerPlayerControl();

    QMediaPlayer::State state() const;
    QMediaPlayer::MediaStatus mediaStatus() const;

    qint64 position() const;
    qint64 duration() const;

    int bufferStatus() const;

    int volume() const;
    bool isMuted() const;

    bool isAudioAvailable() const;
    bool isVideoAvailable() const;
    void setVideoOutput(QObject *output);

    bool isSeekable() const;
    QMediaTimeRange availablePlaybackRanges() const;

    qreal playbackRate() const;
    void setPlaybackRate(qreal rate);

    QMediaContent media() const;
    const QIODevice *mediaStream() const;
    void setMedia(const QMediaContent&, QIODevice *);

    QMediaPlayerResourceSetInterface* resources() const;

public Q_SLOTS:
    void setPosition(qint64 pos);

    void play();
    void pause();
    void stop();

    void setVolume(int volume);
    void setMuted(bool muted);

private Q_SLOTS:
    void updateSessionState(QMediaPlayer::State state);
    void updateMediaStatus();
    void processEOS();
    void setBufferProgress(int progress);

    void handleInvalidMedia();

    void handleResourcesGranted();
    void handleResourcesLost();
    void handleResourcesDenied();

private:
    void playOrPause(QMediaPlayer::State state);

    void pushState();
    void popAndNotifyState();

    QGstreamerPlayerSession *m_session;
    QMediaPlayer::State m_userRequestedState;
    QMediaPlayer::State m_currentState;
    QMediaPlayer::MediaStatus m_mediaStatus;
    QStack<QMediaPlayer::State> m_stateStack;
    QStack<QMediaPlayer::MediaStatus> m_mediaStatusStack;

    int m_bufferProgress;
    qint64 m_pendingSeekPosition;
    bool m_setMediaPending;
    QMediaContent m_currentResource;
    QIODevice *m_stream;

    QMediaPlayerResourceSetInterface *m_resources;
};

QT_END_NAMESPACE

#endif
