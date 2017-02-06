/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include "avfmediaplayercontrol.h"
#include "avfmediaplayersession.h"

QT_USE_NAMESPACE

AVFMediaPlayerControl::AVFMediaPlayerControl(QObject *parent) :
    QMediaPlayerControl(parent)
{
}

AVFMediaPlayerControl::~AVFMediaPlayerControl()
{
#ifdef QT_DEBUG_AVF
    qDebug() << Q_FUNC_INFO;
#endif
}

void AVFMediaPlayerControl::setSession(AVFMediaPlayerSession *session)
{
    m_session = session;

    connect(m_session, SIGNAL(positionChanged(qint64)), this, SIGNAL(positionChanged(qint64)));
    connect(m_session, SIGNAL(durationChanged(qint64)), this, SIGNAL(durationChanged(qint64)));
    connect(m_session, SIGNAL(stateChanged(QMediaPlayer::State)),
            this, SIGNAL(stateChanged(QMediaPlayer::State)));
    connect(m_session, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)),
            this, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)));
    connect(m_session, SIGNAL(volumeChanged(int)), this, SIGNAL(volumeChanged(int)));
    connect(m_session, SIGNAL(mutedChanged(bool)), this, SIGNAL(mutedChanged(bool)));
    connect(m_session, SIGNAL(audioAvailableChanged(bool)), this, SIGNAL(audioAvailableChanged(bool)));
    connect(m_session, SIGNAL(videoAvailableChanged(bool)), this, SIGNAL(videoAvailableChanged(bool)));
    connect(m_session, SIGNAL(error(int,QString)), this, SIGNAL(error(int,QString)));
    connect(m_session, &AVFMediaPlayerSession::playbackRateChanged,
            this, &AVFMediaPlayerControl::playbackRateChanged);
    connect(m_session, &AVFMediaPlayerSession::seekableChanged,
            this, &AVFMediaPlayerControl::seekableChanged);
}

QMediaPlayer::State AVFMediaPlayerControl::state() const
{
    return m_session->state();
}

QMediaPlayer::MediaStatus AVFMediaPlayerControl::mediaStatus() const
{
    return m_session->mediaStatus();
}

QMediaContent AVFMediaPlayerControl::media() const
{
    return m_session->media();
}

const QIODevice *AVFMediaPlayerControl::mediaStream() const
{
    return m_session->mediaStream();
}

void AVFMediaPlayerControl::setMedia(const QMediaContent &content, QIODevice *stream)
{
    m_session->setMedia(content, stream);

    Q_EMIT mediaChanged(content);
}

qint64 AVFMediaPlayerControl::position() const
{
    return m_session->position();
}

qint64 AVFMediaPlayerControl::duration() const
{
    return m_session->duration();
}

int AVFMediaPlayerControl::bufferStatus() const
{
    return m_session->bufferStatus();
}

int AVFMediaPlayerControl::volume() const
{
    return m_session->volume();
}

bool AVFMediaPlayerControl::isMuted() const
{
    return m_session->isMuted();
}

bool AVFMediaPlayerControl::isAudioAvailable() const
{
    return m_session->isAudioAvailable();
}

bool AVFMediaPlayerControl::isVideoAvailable() const
{
    return m_session->isVideoAvailable();
}

bool AVFMediaPlayerControl::isSeekable() const
{
    return m_session->isSeekable();
}

QMediaTimeRange AVFMediaPlayerControl::availablePlaybackRanges() const
{
    return m_session->availablePlaybackRanges();
}

qreal AVFMediaPlayerControl::playbackRate() const
{
    return m_session->playbackRate();
}

void AVFMediaPlayerControl::setPlaybackRate(qreal rate)
{
    m_session->setPlaybackRate(rate);
}

void AVFMediaPlayerControl::setPosition(qint64 pos)
{
    m_session->setPosition(pos);
}

void AVFMediaPlayerControl::play()
{
    m_session->play();
}

void AVFMediaPlayerControl::pause()
{
    m_session->pause();
}

void AVFMediaPlayerControl::stop()
{
    m_session->stop();
}

void AVFMediaPlayerControl::setVolume(int volume)
{
    m_session->setVolume(volume);
}

void AVFMediaPlayerControl::setMuted(bool muted)
{
    m_session->setMuted(muted);
}
