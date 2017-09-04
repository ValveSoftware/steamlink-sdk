/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#ifndef MOCKMEDIAPLAYERCONTROL_H
#define MOCKMEDIAPLAYERCONTROL_H

#include "qmediaplayercontrol.h"

class MockMediaPlayerControl : public QMediaPlayerControl
{
    friend class MockMediaPlayerService;

public:
    MockMediaPlayerControl()
        : QMediaPlayerControl(0)
        , _state(QMediaPlayer::StoppedState)
        , _mediaStatus(QMediaPlayer::NoMedia)
        , _error(QMediaPlayer::NoError)
        , _duration(0)
        , _position(0)
        , _volume(100)
        , _muted(false)
        , _bufferStatus(0)
        , _audioAvailable(false)
        , _videoAvailable(false)
        , _isSeekable(true)
        , _playbackRate(qreal(1.0))
        , _stream(0)
        , _isValid(false)
    {}

    QMediaPlayer::State state() const { return _state; }
    QMediaPlayer::MediaStatus mediaStatus() const { return _mediaStatus; }

    qint64 duration() const { return _duration; }

    qint64 position() const { return _position; }

    void setPosition(qint64 position) { if (position != _position) emit positionChanged(_position = position); }

    int volume() const { return _volume; }
    void setVolume(int volume) { emit volumeChanged(_volume = volume); }

    bool isMuted() const { return _muted; }
    void setMuted(bool muted) { if (muted != _muted) emit mutedChanged(_muted = muted); }

    int bufferStatus() const { return _bufferStatus; }

    bool isAudioAvailable() const { return _audioAvailable; }
    bool isVideoAvailable() const { return _videoAvailable; }

    bool isSeekable() const { return _isSeekable; }
    QMediaTimeRange availablePlaybackRanges() const { return QMediaTimeRange(_seekRange.first, _seekRange.second); }
    void setSeekRange(qint64 minimum, qint64 maximum) { _seekRange = qMakePair(minimum, maximum); }

    qreal playbackRate() const { return _playbackRate; }
    void setPlaybackRate(qreal rate) { if (rate != _playbackRate) emit playbackRateChanged(_playbackRate = rate); }

    QMediaContent media() const { return _media; }
    void setMedia(const QMediaContent &content, QIODevice *stream)
    {
        _stream = stream;
        _media = content;
        _mediaStatus = _media.isNull() ? QMediaPlayer::NoMedia : QMediaPlayer::LoadingMedia;
        if (_state != QMediaPlayer::StoppedState)
            emit stateChanged(_state = QMediaPlayer::StoppedState);
        emit mediaStatusChanged(_mediaStatus);
        emit mediaChanged(_media = content);
    }
    QIODevice *mediaStream() const { return _stream; }

    void play() { if (_isValid && !_media.isNull() && _state != QMediaPlayer::PlayingState) emit stateChanged(_state = QMediaPlayer::PlayingState); }
    void pause() { if (_isValid && !_media.isNull() && _state != QMediaPlayer::PausedState) emit stateChanged(_state = QMediaPlayer::PausedState); }
    void stop() { if (_state != QMediaPlayer::StoppedState) emit stateChanged(_state = QMediaPlayer::StoppedState); }

    QMediaPlayer::State _state;
    QMediaPlayer::MediaStatus _mediaStatus;
    QMediaPlayer::Error _error;
    qint64 _duration;
    qint64 _position;
    int _volume;
    bool _muted;
    int _bufferStatus;
    bool _audioAvailable;
    bool _videoAvailable;
    bool _isSeekable;
    QPair<qint64, qint64> _seekRange;
    qreal _playbackRate;
    QMediaContent _media;
    QIODevice *_stream;
    bool _isValid;
    QString _errorString;
};

#endif // MOCKMEDIAPLAYERCONTROL_H
