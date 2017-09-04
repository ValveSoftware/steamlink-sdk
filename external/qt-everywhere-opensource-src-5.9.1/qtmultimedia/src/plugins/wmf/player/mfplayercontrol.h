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

#ifndef MFPLAYERCONTROL_H
#define MFPLAYERCONTROL_H

#include "qmediacontent.h"
#include "qmediaplayercontrol.h"

#include <QtCore/qcoreevent.h>

#include "mfplayersession.h"

QT_USE_NAMESPACE

class MFPlayerControl : public QMediaPlayerControl
{
    Q_OBJECT
public:
    MFPlayerControl(MFPlayerSession *session);
    ~MFPlayerControl();

    QMediaPlayer::State state() const;

    QMediaPlayer::MediaStatus mediaStatus() const;

    qint64 duration() const;

    qint64 position() const;
    void setPosition(qint64 position);

    int volume() const;
    void setVolume(int volume);

    bool isMuted() const;
    void setMuted(bool muted);

    int bufferStatus() const;

    bool isAudioAvailable() const;
    bool isVideoAvailable() const;

    bool isSeekable() const;

    QMediaTimeRange availablePlaybackRanges() const;

    qreal playbackRate() const;
    void setPlaybackRate(qreal rate);

    QMediaContent media() const;
    const QIODevice *mediaStream() const;
    void setMedia(const QMediaContent &media, QIODevice *stream);

    void play();
    void pause();
    void stop();

private Q_SLOTS:
    void handleStatusChanged();
    void handleVideoAvailable();
    void handleAudioAvailable();
    void handleDurationUpdate(qint64 duration);
    void handleSeekableUpdate(bool seekable);
    void handleError(QMediaPlayer::Error errorCode, const QString& errorString, bool isFatal);

private:
    void changeState(QMediaPlayer::State state);
    void resetAudioVideoAvailable();
    void refreshState();

    QMediaPlayer::State m_state;
    bool m_stateDirty;
    QMediaPlayer::MediaStatus m_status;
    QMediaPlayer::Error m_error;

    bool     m_videoAvailable;
    bool     m_audioAvailable;
    qint64   m_duration;
    bool     m_seekable;

    QIODevice *m_stream;
    QMediaContent m_media;
    MFPlayerSession *m_session;
};

#endif
