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

#ifndef DIRECTSHOWPLAYERCONTROL_H
#define DIRECTSHOWPLAYERCONTROL_H

#include <dshow.h>

#include "qmediacontent.h"
#include "qmediaplayercontrol.h"

#include <QtCore/qcoreevent.h>

#include "directshowplayerservice.h"

QT_USE_NAMESPACE

class DirectShowPlayerControl : public QMediaPlayerControl
{
    Q_OBJECT
public:
    DirectShowPlayerControl(DirectShowPlayerService *service, QObject *parent = 0);
    ~DirectShowPlayerControl();

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

    void updateState(QMediaPlayer::State state);
    void updateStatus(QMediaPlayer::MediaStatus status);
    void updateMediaInfo(qint64 duration, int streamTypes, bool seekable);
    void updatePlaybackRate(qreal rate);
    void updateAudioOutput(IBaseFilter *filter);
    void updateError(QMediaPlayer::Error error, const QString &errorString);
    void updatePosition(qint64 position);

protected:
    void customEvent(QEvent *event);

private:
    enum Properties
    {
        StateProperty        = 0x01,
        StatusProperty       = 0x02,
        StreamTypesProperty  = 0x04,
        DurationProperty     = 0x08,
        PlaybackRateProperty = 0x10,
        SeekableProperty     = 0x20,
        ErrorProperty        = 0x40,
        PositionProperty     = 0x80
    };

    enum Event
    {
        PropertiesChanged = QEvent::User
    };

    void playOrPause(QMediaPlayer::State state);

    void scheduleUpdate(int properties);
    void emitPropertyChanges();
    void setVolumeHelper(int volume);

    DirectShowPlayerService *m_service;
    IBasicAudio *m_audio;
    QIODevice *m_stream;
    int m_updateProperties;
    QMediaPlayer::State m_state;
    QMediaPlayer::MediaStatus m_status;
    QMediaPlayer::Error m_error;
    int m_streamTypes;
    int m_volume;
    bool m_muted;
    qint64 m_emitPosition;
    qint64 m_pendingPosition;
    qint64 m_duration;
    qreal m_playbackRate;
    bool m_seekable;
    QMediaContent m_media;
    QString m_errorString;

};

#endif
