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

#include "qmediaplayercontrol.h"
#include "qmediacontrol_p.h"
#include "qmediaplayer.h"

QT_BEGIN_NAMESPACE


/*!
    \class QMediaPlayerControl
    \inmodule QtMultimedia


    \ingroup multimedia_control


    \brief The QMediaPlayerControl class provides access to the media playing
    functionality of a QMediaService.

    If a QMediaService can play media is will implement QMediaPlayerControl.
    This control provides a means to set the \l {setMedia()}{media} to play,
    \l {play()}{start}, \l {pause()} {pause} and \l {stop()}{stop} playback,
    \l {setPosition()}{seek}, and control the \l {setVolume()}{volume}.
    It also provides feedback on the \l {duration()}{duration} of the media,
    the current \l {position()}{position}, and \l {bufferStatus()}{buffering}
    progress.

    The functionality provided by this control is exposed to application
    code through the QMediaPlayer class.

    The interface name of QMediaPlayerControl is \c org.qt-project.qt.mediaplayercontrol/5.0 as
    defined in QMediaPlayerControl_iid.

    \sa QMediaService::requestControl(), QMediaPlayer
*/

/*!
    \macro QMediaPlayerControl_iid

    \c org.qt-project.qt.mediaplayercontrol/5.0

    Defines the interface name of the QMediaPlayerControl class.

    \relates QMediaPlayerControl
*/

/*!
    Destroys a media player control.
*/
QMediaPlayerControl::~QMediaPlayerControl()
{
}

/*!
    Constructs a new media player control with the given \a parent.
*/
QMediaPlayerControl::QMediaPlayerControl(QObject *parent):
    QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
    \fn QMediaPlayerControl::state() const

    Returns the state of a player control.
*/

/*!
    \fn QMediaPlayerControl::stateChanged(QMediaPlayer::State state)

    Signals that the \a state of a player control has changed.

    \sa state()
*/

/*!
    \fn QMediaPlayerControl::mediaStatus() const

    Returns the status of the current media.
*/


/*!
    \fn QMediaPlayerControl::mediaStatusChanged(QMediaPlayer::MediaStatus status)

    Signals that the \a status of the current media has changed.

    \sa mediaStatus()
*/


/*!
    \fn QMediaPlayerControl::duration() const

    Returns the duration of the current media in milliseconds.
*/

/*!
    \fn QMediaPlayerControl::durationChanged(qint64 duration)

    Signals that the \a duration of the current media has changed.

    \sa duration()
*/

/*!
    \fn QMediaPlayerControl::position() const

    Returns the current playback position in milliseconds.
*/

/*!
    \fn QMediaPlayerControl::setPosition(qint64 position)

    Sets the playback \a position of the current media.  This will initiate a seek and it may take
    some time for playback to reach the position set.
*/

/*!
    \fn QMediaPlayerControl::positionChanged(qint64 position)

    Signals the playback \a position has changed.

    This is only emitted in when there has been a discontinous change in the playback postion, such
    as a seek or the position being reset.

    \sa position()
*/

/*!
    \fn QMediaPlayerControl::volume() const

    Returns the audio volume of a player control.
*/

/*!
    \fn QMediaPlayerControl::setVolume(int volume)

    Sets the audio \a volume of a player control.

    The volume is scaled linearly, ranging from \c 0 (silence) to \c 100 (full volume).
*/

/*!
    \fn QMediaPlayerControl::volumeChanged(int volume)

    Signals the audio \a volume of a player control has changed.

    \sa volume()
*/

/*!
    \fn QMediaPlayerControl::isMuted() const

    Returns the mute state of a player control.
*/

/*!
    \fn QMediaPlayerControl::setMuted(bool mute)

    Sets the \a mute state of a player control.
*/

/*!
    \fn QMediaPlayerControl::mutedChanged(bool mute)

    Signals a change in the \a mute status of a player control.

    \sa isMuted()
*/

/*!
    \fn QMediaPlayerControl::bufferStatus() const

    Returns the buffering progress of the current media.  Progress is measured in the percentage
    of the buffer filled.
*/

/*!
    \fn QMediaPlayerControl::bufferStatusChanged(int progress)

    Signals that buffering \a progress has changed.

    \sa bufferStatus()
*/

/*!
    \fn QMediaPlayerControl::isAudioAvailable() const

    Identifies if there is audio output available for the current media.

    Returns true if audio output is available and false otherwise.
*/

/*!
    \fn QMediaPlayerControl::audioAvailableChanged(bool audio)

    Signals that there has been a change in the availability of \a audio output.

    \sa isAudioAvailable()
*/

/*!
    \fn QMediaPlayerControl::isVideoAvailable() const

    Identifies if there is video output available for the current media.

    Returns true if video output is available and false otherwise.
*/

/*!
    \fn QMediaPlayerControl::videoAvailableChanged(bool video)

    Signals that there has been a change in the availability of \a video output.

    \sa isVideoAvailable()
*/

/*!
    \fn QMediaPlayerControl::isSeekable() const

    Identifies if the current media is seekable.

    Returns true if it possible to seek within the current media, and false otherwise.
*/

/*!
    \fn QMediaPlayerControl::seekableChanged(bool seekable)

    Signals that the \a seekable state of a player control has changed.

    \sa isSeekable()
*/

/*!
    \fn QMediaPlayerControl::availablePlaybackRanges() const

    Returns a range of times in milliseconds that can be played back.

    Usually for local files this is a continuous interval equal to [0..duration()]
    or an empty time range if seeking is not supported, but for network sources
    it refers to the buffered parts of the media.
*/

/*!
    \fn QMediaPlayerControl::availablePlaybackRangesChanged(const QMediaTimeRange &ranges)

    Signals that the available media playback \a ranges have changed.

    \sa QMediaPlayerControl::availablePlaybackRanges()
*/

/*!
    \fn qreal QMediaPlayerControl::playbackRate() const

    Returns the rate of playback.
*/

/*!
    \fn QMediaPlayerControl::setPlaybackRate(qreal rate)

    Sets the \a rate of playback.
*/

/*!
    \fn QMediaPlayerControl::media() const

    Returns the current media source.
*/

/*!
    \fn QMediaPlayerControl::mediaStream() const

    Returns the current media stream. This is only a valid if a stream was passed to setMedia().

    \sa setMedia()
*/

/*!
    \fn QMediaPlayerControl::setMedia(const QMediaContent &media, QIODevice *stream)

    Sets the current \a media source.  If a \a stream is supplied; data will be read from that
    instead of attempting to resolve the media source.  The media source may still be used to
    supply media information such as mime type.

    Setting the media to a null QMediaContent will cause the control to discard all
    information relating to the current media source and to cease all I/O operations related
    to that media.

    Qt resource files are never passed as is. If the service supports
    QMediaServiceProviderHint::StreamPlayback, a \a stream is supplied, pointing to an opened
    QFile. Otherwise, the resource is copied into a temporary file and \a media contains the
    url to that file.
*/

/*!
    \fn QMediaPlayerControl::mediaChanged(const QMediaContent& content)

    Signals that the current media \a content has changed.
*/

/*!
    \fn QMediaPlayerControl::play()

    Starts playback of the current media.

    If successful the player control will immediately enter the \l {QMediaPlayer::PlayingState}
    {playing} state.

    \sa state()
*/

/*!
    \fn QMediaPlayerControl::pause()

    Pauses playback of the current media.

    If sucessful the player control will immediately enter the \l {QMediaPlayer::PausedState}
    {paused} state.

    \sa state(), play(), stop()
*/

/*!
    \fn QMediaPlayerControl::stop()

    Stops playback of the current media.

    If successful the player control will immediately enter the \l {QMediaPlayer::StoppedState}
    {stopped} state.
*/

/*!
    \fn QMediaPlayerControl::error(int error, const QString &errorString)

    Signals that an \a error has occurred.  The \a errorString provides a more detailed explanation.
*/

/*!
    \fn QMediaPlayerControl::playbackRateChanged(qreal rate)

    Signal emitted when playback rate changes to \a rate.
*/

#include "moc_qmediaplayercontrol.cpp"
QT_END_NAMESPACE

