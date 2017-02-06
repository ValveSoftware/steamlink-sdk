/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Mobility Components.
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

#include "mfplayercontrol.h"
#include <qtcore/qdebug.h>

//#define DEBUG_MEDIAFOUNDATION

MFPlayerControl::MFPlayerControl(MFPlayerSession *session)
: QMediaPlayerControl(session)
, m_state(QMediaPlayer::StoppedState)
, m_stateDirty(false)
, m_videoAvailable(false)
, m_audioAvailable(false)
, m_duration(-1)
, m_seekable(false)
, m_session(session)
{
    QObject::connect(m_session, SIGNAL(statusChanged()), this, SLOT(handleStatusChanged()));
    QObject::connect(m_session, SIGNAL(videoAvailable()), this, SLOT(handleVideoAvailable()));
    QObject::connect(m_session, SIGNAL(audioAvailable()), this, SLOT(handleAudioAvailable()));
    QObject::connect(m_session, SIGNAL(durationUpdate(qint64)), this, SLOT(handleDurationUpdate(qint64)));
    QObject::connect(m_session, SIGNAL(seekableUpdate(bool)), this, SLOT(handleSeekableUpdate(bool)));
    QObject::connect(m_session, SIGNAL(error(QMediaPlayer::Error,QString,bool)), this, SLOT(handleError(QMediaPlayer::Error,QString,bool)));
    QObject::connect(m_session, SIGNAL(positionChanged(qint64)), this, SIGNAL(positionChanged(qint64)));
    QObject::connect(m_session, SIGNAL(volumeChanged(int)), this, SIGNAL(volumeChanged(int)));
    QObject::connect(m_session, SIGNAL(mutedChanged(bool)), this, SIGNAL(mutedChanged(bool)));
    QObject::connect(m_session, SIGNAL(playbackRateChanged(qreal)), this, SIGNAL(playbackRateChanged(qreal)));
    QObject::connect(m_session, SIGNAL(bufferStatusChanged(int)), this, SIGNAL(bufferStatusChanged(int)));
}

MFPlayerControl::~MFPlayerControl()
{
}

void MFPlayerControl::setMedia(const QMediaContent &media, QIODevice *stream)
{
    if (m_state != QMediaPlayer::StoppedState) {
        changeState(QMediaPlayer::StoppedState);
        m_session->stop(true);
        refreshState();
    }

    m_media = media;
    m_stream = stream;
    resetAudioVideoAvailable();
    handleDurationUpdate(-1);
    handleSeekableUpdate(false);
    m_session->load(media, stream);
    emit mediaChanged(m_media);
}

void MFPlayerControl::play()
{
    if (m_state == QMediaPlayer::PlayingState)
        return;
    if (QMediaPlayer::InvalidMedia == m_session->status())
        m_session->load(m_media, m_stream);

    switch (m_session->status()) {
    case QMediaPlayer::UnknownMediaStatus:
    case QMediaPlayer::NoMedia:
    case QMediaPlayer::InvalidMedia:
        return;
    case QMediaPlayer::LoadedMedia:
    case QMediaPlayer::BufferingMedia:
    case QMediaPlayer::BufferedMedia:
    case QMediaPlayer::EndOfMedia:
        changeState(QMediaPlayer::PlayingState);
        m_session->start();
        break;
    default: //Loading/Stalled
        changeState(QMediaPlayer::PlayingState);
        break;
    }
    refreshState();
}

void MFPlayerControl::pause()
{
    if (m_state != QMediaPlayer::PlayingState)
        return;
    changeState(QMediaPlayer::PausedState);
    m_session->pause();
    refreshState();
}

void MFPlayerControl::stop()
{
    if (m_state == QMediaPlayer::StoppedState)
        return;
    changeState(QMediaPlayer::StoppedState);
    m_session->stop();
    refreshState();
}

void MFPlayerControl::changeState(QMediaPlayer::State state)
{
    if (m_state == state)
        return;
    m_state = state;
    m_stateDirty = true;
}

void MFPlayerControl::refreshState()
{
    if (!m_stateDirty)
        return;
    m_stateDirty = false;
#ifdef DEBUG_MEDIAFOUNDATION
    qDebug() << "MFPlayerControl::emit stateChanged" << m_state;
#endif
    emit stateChanged(m_state);
}

void MFPlayerControl::handleStatusChanged()
{
    QMediaPlayer::MediaStatus status = m_session->status();
    switch (status) {
    case QMediaPlayer::EndOfMedia:
        changeState(QMediaPlayer::StoppedState);
        break;
    case QMediaPlayer::InvalidMedia:
        break;
    case QMediaPlayer::LoadedMedia:
    case QMediaPlayer::BufferingMedia:
    case QMediaPlayer::BufferedMedia:
        if (m_state == QMediaPlayer::PlayingState)
            m_session->start();
        break;
    }
    emit mediaStatusChanged(m_session->status());
    refreshState();
}

void MFPlayerControl::handleVideoAvailable()
{
    if (m_videoAvailable)
        return;
    m_videoAvailable = true;
    emit videoAvailableChanged(m_videoAvailable);
}

void MFPlayerControl::handleAudioAvailable()
{
    if (m_audioAvailable)
        return;
    m_audioAvailable = true;
    emit audioAvailableChanged(m_audioAvailable);
}

void MFPlayerControl::resetAudioVideoAvailable()
{
    bool videoDirty = false;
    if (m_videoAvailable) {
        m_videoAvailable = false;
        videoDirty = true;
    }
    if (m_audioAvailable) {
        m_audioAvailable = false;
        emit audioAvailableChanged(m_audioAvailable);
    }
    if (videoDirty)
        emit videoAvailableChanged(m_videoAvailable);
}

void MFPlayerControl::handleDurationUpdate(qint64 duration)
{
    if (m_duration == duration)
        return;
    m_duration = duration;
    emit durationChanged(m_duration);
}

void MFPlayerControl::handleSeekableUpdate(bool seekable)
{
    if (m_seekable == seekable)
        return;
    m_seekable = seekable;
    emit seekableChanged(m_seekable);
}

QMediaPlayer::State MFPlayerControl::state() const
{
    return m_state;
}

QMediaPlayer::MediaStatus MFPlayerControl::mediaStatus() const
{
    return m_session->status();
}

qint64 MFPlayerControl::duration() const
{
    return m_duration;
}

qint64 MFPlayerControl::position() const
{
    return m_session->position();
}

void MFPlayerControl::setPosition(qint64 position)
{
    if (!m_seekable || position == m_session->position())
        return;
    m_session->setPosition(position);
}

int MFPlayerControl::volume() const
{
    return m_session->volume();
}

void MFPlayerControl::setVolume(int volume)
{
    m_session->setVolume(volume);
}

bool MFPlayerControl::isMuted() const
{
    return m_session->isMuted();
}

void MFPlayerControl::setMuted(bool muted)
{
    m_session->setMuted(muted);
}

int MFPlayerControl::bufferStatus() const
{
    return m_session->bufferStatus();
}

bool MFPlayerControl::isAudioAvailable() const
{
    return m_audioAvailable;
}

bool MFPlayerControl::isVideoAvailable() const
{
    return m_videoAvailable;
}

bool MFPlayerControl::isSeekable() const
{
    return m_seekable;
}

QMediaTimeRange MFPlayerControl::availablePlaybackRanges() const
{
    return m_session->availablePlaybackRanges();
}

qreal MFPlayerControl::playbackRate() const
{
    return m_session->playbackRate();
}

void MFPlayerControl::setPlaybackRate(qreal rate)
{
    m_session->setPlaybackRate(rate);
}

QMediaContent MFPlayerControl::media() const
{
    return m_media;
}

const QIODevice* MFPlayerControl::mediaStream() const
{
    return m_stream;
}

void MFPlayerControl::handleError(QMediaPlayer::Error errorCode, const QString& errorString, bool isFatal)
{
    if (isFatal)
        stop();
    emit error(int(errorCode), errorString);
}
