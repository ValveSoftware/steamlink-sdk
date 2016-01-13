/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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
#include <QtQml/qqmlinfo.h>

#include "qdeclarativeaudio_p.h"

#include <qmediaplayercontrol.h>
#include <qmediaavailabilitycontrol.h>

#include <qmediaservice.h>
#include <private/qmediaserviceprovider_p.h>
#include <qmetadatareadercontrol.h>
#include <qmediaavailabilitycontrol.h>

#include "qdeclarativemediametadata_p.h"

#include <QTimerEvent>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Audio
    \instantiates QDeclarativeAudio
    \brief Add audio playback to a scene.

    \inqmlmodule QtMultimedia
    \ingroup multimedia_qml
    \ingroup multimedia_audio_qml

    This type is part of the \b{QtMultimedia 5.0} module.

    \qml
    import QtQuick 2.0
    import QtMultimedia 5.0

    Text {
        text: "Click Me!";
        font.pointSize: 24;
        width: 150; height: 50;

        Audio {
            id: playMusic
            source: "music.wav"
        }
        MouseArea {
            id: playArea
            anchors.fill: parent
            onPressed:  { playMusic.play() }
        }
    }
    \endqml

    \sa Video
*/

void QDeclarativeAudio::_q_error(QMediaPlayer::Error errorCode)
{
    m_error = errorCode;
    m_errorString = m_player->errorString();

    emit error(Error(errorCode), m_errorString);
    emit errorChanged();
}

void QDeclarativeAudio::_q_availabilityChanged(QMultimedia::AvailabilityStatus)
{
    emit availabilityChanged(availability());
}

QDeclarativeAudio::QDeclarativeAudio(QObject *parent)
    : QObject(parent)
    , m_autoPlay(false)
    , m_autoLoad(true)
    , m_loaded(false)
    , m_muted(false)
    , m_complete(false)
    , m_loopCount(1)
    , m_runningCount(0)
    , m_position(0)
    , m_vol(1.0)
    , m_playbackRate(1.0)
    , m_playbackState(QMediaPlayer::StoppedState)
    , m_status(QMediaPlayer::NoMedia)
    , m_error(QMediaPlayer::ServiceMissingError)
    , m_player(0)
{
}

QDeclarativeAudio::~QDeclarativeAudio()
{
    m_metaData.reset();
    delete m_player;
}

/*!
    \qmlproperty enumeration QtMultimedia::Audio::availability

    Returns the availability state of the media player.

    This is one of:
    \table
    \header \li Value \li Description
    \row \li Available
        \li The media player is available to use.
    \row \li Busy
        \li The media player is usually available, but some other
           process is utilizing the hardware necessary to play media.
    \row \li Unavailable
        \li There are no supported media playback facilities.
    \row \li ResourceMissing
        \li There is one or more resources missing, so the media player cannot
           be used.  It may be possible to try again at a later time.
    \endtable
 */
QDeclarativeAudio::Availability QDeclarativeAudio::availability() const
{
    if (!m_player)
        return Unavailable;
    return Availability(m_player->availability());
}

QUrl QDeclarativeAudio::source() const
{
    return m_source;
}

bool QDeclarativeAudio::autoPlay() const
{
    return m_autoPlay;
}

void QDeclarativeAudio::setAutoPlay(bool autoplay)
{
    if (m_autoPlay == autoplay)
        return;

    m_autoPlay = autoplay;
    emit autoPlayChanged();
}


void QDeclarativeAudio::setSource(const QUrl &url)
{
    if (url == m_source)
        return;

    m_source = url;
    m_content = m_source.isEmpty() ? QMediaContent() : m_source;
    m_loaded = false;
    if (m_complete && (m_autoLoad || m_content.isNull() || m_autoPlay)) {
        if (m_error != QMediaPlayer::ServiceMissingError && m_error != QMediaPlayer::NoError) {
            m_error = QMediaPlayer::NoError;
            m_errorString = QString();

            emit errorChanged();
        }

        m_player->setMedia(m_content, 0);
        m_loaded = true;
    }
    else
        emit sourceChanged();

    if (m_autoPlay)
        m_player->play();
}

bool QDeclarativeAudio::isAutoLoad() const
{
    return m_autoLoad;
}

void QDeclarativeAudio::setAutoLoad(bool autoLoad)
{
    if (m_autoLoad == autoLoad)
        return;

    m_autoLoad = autoLoad;
    emit autoLoadChanged();
}

int QDeclarativeAudio::loopCount() const
{
    return m_loopCount;
}

void QDeclarativeAudio::setLoopCount(int loopCount)
{
    if (loopCount == 0)
        loopCount = 1;
    else if (loopCount < -1)
        loopCount = -1;

    if (m_loopCount == loopCount) {
        return;
    }
    m_loopCount = loopCount;
    m_runningCount = loopCount - 1;
    emit loopCountChanged();
}

void QDeclarativeAudio::setPlaybackState(QMediaPlayer::State playbackState)
{
    if (m_playbackState == playbackState)
        return;

    if (m_complete) {
        switch (playbackState){
        case (QMediaPlayer::PlayingState):
            if (!m_loaded) {
                m_player->setMedia(m_content, 0);
                m_player->setPosition(m_position);
                m_loaded = true;
            }
            m_player->play();
            break;

        case (QMediaPlayer::PausedState):
            if (!m_loaded) {
                m_player->setMedia(m_content, 0);
                m_player->setPosition(m_position);
                m_loaded = true;
            }
            m_player->pause();
            break;

        case (QMediaPlayer::StoppedState):
            m_player->stop();
        }
    }
}

int QDeclarativeAudio::duration() const
{
    return !m_complete ? 0 : m_player->duration();
}

int QDeclarativeAudio::position() const
{
    return !m_complete ? m_position : m_player->position();
}

qreal QDeclarativeAudio::volume() const
{
    return !m_complete ? m_vol : qreal(m_player->volume()) / 100;
}

void QDeclarativeAudio::setVolume(qreal volume)
{
    if (volume < 0 || volume > 1) {
        qmlInfo(this) << tr("volume should be between 0.0 and 1.0");
        return;
    }

    if (this->volume() == volume)
        return;

    if (m_complete) {
        m_player->setVolume(qRound(volume * 100));
    } else {
        m_vol = volume;
        emit volumeChanged();
    }
}

bool QDeclarativeAudio::isMuted() const
{
    return !m_complete ? m_muted : m_player->isMuted();
}

void QDeclarativeAudio::setMuted(bool muted)
{
    if (isMuted() == muted)
        return;

    if (m_complete) {
        m_player->setMuted(muted);
    } else {
        m_muted = muted;
        emit mutedChanged();
    }
}

qreal QDeclarativeAudio::bufferProgress() const
{
    return !m_complete ? 0 : qreal(m_player->bufferStatus()) / 100;
}

bool QDeclarativeAudio::isSeekable() const
{
    return !m_complete ? false : m_player->isSeekable();
}

qreal QDeclarativeAudio::playbackRate() const
{
    return m_complete ? m_player->playbackRate() : m_playbackRate;
}

void QDeclarativeAudio::setPlaybackRate(qreal rate)
{
    if (playbackRate() == rate)
        return;

    if (m_complete) {
        m_player->setPlaybackRate(rate);
    } else {
        m_playbackRate = rate;
        emit playbackRateChanged();
    }
}

QString QDeclarativeAudio::errorString() const
{
    return m_errorString;
}

QDeclarativeMediaMetaData *QDeclarativeAudio::metaData() const
{
    return m_metaData.data();
}


/*!
    \qmlmethod QtMultimedia::Audio::play()

    Starts playback of the media.

    Sets the \l playbackState property to PlayingState.
*/

void QDeclarativeAudio::play()
{
    if (!m_complete)
        return;

    setPlaybackState(QMediaPlayer::PlayingState);
}

/*!
    \qmlmethod QtMultimedia::Audio::pause()

    Pauses playback of the media.

    Sets the \l playbackState property to PausedState.
*/

void QDeclarativeAudio::pause()
{
    if (!m_complete)
        return;

    setPlaybackState(QMediaPlayer::PausedState);
}

/*!
    \qmlmethod QtMultimedia::Audio::stop()

    Stops playback of the media.

    Sets the \l playbackState property to StoppedState.
*/

void QDeclarativeAudio::stop()
{
    if (!m_complete)
        return;

    setPlaybackState(QMediaPlayer::StoppedState);
}

/*!
    \qmlmethod QtMultimedia::Audio::seek(offset)

    If the \l seekable property is true, seeks the current
    playback position to \a offset.

    Seeking may be asynchronous, so the \l position property
    may not be updated immediately.

    \sa seekable, position
*/
void QDeclarativeAudio::seek(int position)
{
    // QMediaPlayer clamps this to positive numbers
    if (position < 0)
        position = 0;

    if (this->position() == position)
        return;

    if (m_complete) {
        m_player->setPosition(position);
    } else {
        m_position = position;
        emit positionChanged();
    }
}

/*!
    \qmlproperty url QtMultimedia::Audio::source

    This property holds the source URL of the media.
*/

/*!
    \qmlproperty int QtMultimedia::Audio::loops

    This property holds the number of times the media is played. A value of \c 0 or \c 1 means
    the media will be played only once; set to \c Audio.Infinite to enable infinite looping.

    The value can be changed while the media is playing, in which case it will update
    the remaining loops to the new value.

    The default is \c 1.
*/

/*!
    \qmlproperty bool QtMultimedia::Audio::autoLoad

    This property indicates if loading of media should begin immediately.

    Defaults to \c true. If \c false, the media will not be loaded until playback is started.
*/

/*!
    \qmlsignal QtMultimedia::Audio::playbackStateChanged()

    This signal is emitted when the \l playbackState property is altered.

    The corresponding handler is \c onPlaybackStateChanged.
*/


/*!
    \qmlsignal QtMultimedia::Audio::paused()

    This signal is emitted when playback is paused.

    The corresponding handler is \c onPaused.
*/

/*!
    \qmlsignal QtMultimedia::Audio::stopped()

    This signal is emitted when playback is stopped.

    The corresponding handler is \c onStopped.
*/

/*!
    \qmlsignal QtMultimedia::Audio::playing()

    This signal is emitted when playback is started or resumed.

    The corresponding handler is \c onPlaying.
*/

/*!
    \qmlproperty enumeration QtMultimedia::Audio::status

    This property holds the status of media loading. It can be one of:

    \list
    \li NoMedia - no media has been set.
    \li Loading - the media is currently being loaded.
    \li Loaded - the media has been loaded.
    \li Buffering - the media is buffering data.
    \li Stalled - playback has been interrupted while the media is buffering data.
    \li Buffered - the media has buffered data.
    \li EndOfMedia - the media has played to the end.
    \li InvalidMedia - the media cannot be played.
    \li UnknownStatus - the status of the media is unknown.
    \endlist
*/

QDeclarativeAudio::Status QDeclarativeAudio::status() const
{
    return Status(m_status);
}


/*!
    \qmlproperty enumeration QtMultimedia::Audio::playbackState

    This property holds the state of media playback. It can be one of:

    \list
    \li PlayingState - the media is currently playing.
    \li PausedState - playback of the media has been suspended.
    \li StoppedState - playback of the media is yet to begin.
    \endlist
*/

QDeclarativeAudio::PlaybackState QDeclarativeAudio::playbackState() const
{
    return PlaybackState(m_playbackState);
}

/*!
    \qmlproperty bool QtMultimedia::Audio::autoPlay

    This property controls whether the media will begin to play on start up.

    Defaults to \c false. If set to \c true, the value of autoLoad will be overwritten to \c true.
*/

/*!
    \qmlproperty int QtMultimedia::Audio::duration

    This property holds the duration of the media in milliseconds.

    If the media doesn't have a fixed duration (a live stream for example) this will be 0.
*/

/*!
    \qmlproperty int QtMultimedia::Audio::position

    This property holds the current playback position in milliseconds.

    To change this position, use the \l seek() method.

    \sa seek()
*/

/*!
    \qmlproperty real QtMultimedia::Audio::volume

    This property holds the volume of the audio output, from 0.0 (silent) to 1.0 (maximum volume).

    Defaults to 1.0.
*/

/*!
    \qmlproperty bool QtMultimedia::Audio::muted

    This property holds whether the audio output is muted.

    Defaults to false.
*/

/*!
    \qmlproperty bool QtMultimedia::Audio::hasAudio

    This property holds whether the media contains audio.
*/

bool QDeclarativeAudio::hasAudio() const
{
    return !m_complete ? false : m_player->isAudioAvailable();
}

/*!
    \qmlproperty bool QtMultimedia::Audio::hasVideo

    This property holds whether the media contains video.
*/

bool QDeclarativeAudio::hasVideo() const
{
    return !m_complete ? false : m_player->isVideoAvailable();
}

/*!
    \qmlproperty real QtMultimedia::Audio::bufferProgress

    This property holds how much of the data buffer is currently filled, from \c 0.0 (empty) to
    \c 1.0 (full).

    Playback can start or resume only when the buffer is entirely filled, in which case the
    status is \c Audio.Buffered or \c Audio.Buffering. A value lower than \c 1.0 implies that
    the status is \c Audio.Stalled.

    \sa status
*/

/*!
    \qmlproperty bool QtMultimedia::Audio::seekable

    This property holds whether position of the audio can be changed.

    If true, calling the \l seek() method will cause playback to seek to the new position.
*/

/*!
    \qmlproperty real QtMultimedia::Audio::playbackRate

    This property holds the rate at which audio is played at as a multiple of the normal rate.

    Defaults to 1.0.
*/

/*!
    \qmlproperty enumeration QtMultimedia::Audio::error

    This property holds the error state of the audio.  It can be one of:

    \table
    \header \li Value \li Description
    \row \li NoError
        \li There is no current error.
    \row \li ResourceError
        \li The audio cannot be played due to a problem allocating resources.
    \row \li FormatError
        \li The audio format is not supported.
    \row \li NetworkError
        \li The audio cannot be played due to network issues.
    \row \li AccessDenied
        \li The audio cannot be played due to insufficient permissions.
    \row \li ServiceMissing
        \li The audio cannot be played because the media service could not be
    instantiated.
    \endtable
*/

QDeclarativeAudio::Error QDeclarativeAudio::error() const
{
    return Error(m_error);
}

void QDeclarativeAudio::classBegin()
{
    m_player = new QMediaPlayer(this);

    connect(m_player, SIGNAL(stateChanged(QMediaPlayer::State)),
            this, SLOT(_q_statusChanged()));
    connect(m_player, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)),
            this, SLOT(_q_statusChanged()));
    connect(m_player, SIGNAL(mediaChanged(QMediaContent)),
            this, SIGNAL(sourceChanged()));
    connect(m_player, SIGNAL(durationChanged(qint64)),
            this, SIGNAL(durationChanged()));
    connect(m_player, SIGNAL(positionChanged(qint64)),
            this, SIGNAL(positionChanged()));
    connect(m_player, SIGNAL(volumeChanged(int)),
            this, SIGNAL(volumeChanged()));
    connect(m_player, SIGNAL(mutedChanged(bool)),
            this, SIGNAL(mutedChanged()));
    connect(m_player, SIGNAL(bufferStatusChanged(int)),
            this, SIGNAL(bufferProgressChanged()));
    connect(m_player, SIGNAL(seekableChanged(bool)),
            this, SIGNAL(seekableChanged()));
    connect(m_player, SIGNAL(playbackRateChanged(qreal)),
            this, SIGNAL(playbackRateChanged()));
    connect(m_player, SIGNAL(error(QMediaPlayer::Error)),
            this, SLOT(_q_error(QMediaPlayer::Error)));
    connect(m_player, SIGNAL(audioAvailableChanged(bool)),
            this, SIGNAL(hasAudioChanged()));
    connect(m_player, SIGNAL(videoAvailableChanged(bool)),
            this, SIGNAL(hasVideoChanged()));

    m_error = m_player->availability() == QMultimedia::ServiceMissing ? QMediaPlayer::ServiceMissingError : QMediaPlayer::NoError;

    connect(m_player, SIGNAL(availabilityChanged(QMultimedia::AvailabilityStatus)),
                     this, SLOT(_q_availabilityChanged(QMultimedia::AvailabilityStatus)));

    m_metaData.reset(new QDeclarativeMediaMetaData(m_player));

    connect(m_player, SIGNAL(metaDataChanged()),
            m_metaData.data(), SIGNAL(metaDataChanged()));

    emit mediaObjectChanged();
}

void QDeclarativeAudio::componentComplete()
{
    if (!qFuzzyCompare(m_vol, qreal(1.0)))
        m_player->setVolume(m_vol * 100);
    if (m_muted)
        m_player->setMuted(m_muted);
    if (!qFuzzyCompare(m_playbackRate, qreal(1.0)))
        m_player->setPlaybackRate(m_playbackRate);

    if (!m_content.isNull() && (m_autoLoad || m_autoPlay)) {
        m_player->setMedia(m_content, 0);
        m_loaded = true;
        if (m_position > 0)
            m_player->setPosition(m_position);
    }

    m_complete = true;

    if (m_autoPlay) {
        if (m_content.isNull()) {
            m_player->stop();
        } else {
            m_player->play();
        }
    }
}

void QDeclarativeAudio::_q_statusChanged()
{
    if (m_player->mediaStatus() == QMediaPlayer::EndOfMedia && m_runningCount != 0) {
        m_runningCount -= 1;
        m_player->play();
    }
    const QMediaPlayer::MediaStatus oldStatus = m_status;
    const QMediaPlayer::State lastPlaybackState = m_playbackState;

    const QMediaPlayer::State state = m_player->state();

    m_playbackState = state;

    m_status = m_player->mediaStatus();

    if (m_status != oldStatus)
        emit statusChanged();

    if (lastPlaybackState != state) {

        if (lastPlaybackState == QMediaPlayer::StoppedState)
            m_runningCount = m_loopCount - 1;

        switch (state) {
        case QMediaPlayer::StoppedState:
            emit stopped();
            break;
        case QMediaPlayer::PausedState:
            emit paused();
            break;
        case QMediaPlayer::PlayingState:
            emit playing();
            break;
        }

        emit playbackStateChanged();
    }
}

/*!
    \qmlproperty string QtMultimedia::Audio::errorString

    This property holds a string describing the current error condition in more detail.
*/

/*!
    \qmlsignal QtMultimedia::Audio::error(error, errorString)

    This signal is emitted when an \l {QMediaPlayer::Error}{error} has
    occurred.  The errorString parameter may contain more detailed
    information about the error.

    The corresponding handler is \c onError.
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.title

    This property holds the title of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.subTitle

    This property holds the sub-title of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.author

    This property holds the author of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.comment

    This property holds a user comment about the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.description

    This property holds a description of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.category

    This property holds the category of the media

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.genre

    This property holds the genre of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.year

    This property holds the year of release of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.date

    This property holds the date of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.userRating

    This property holds a user rating of the media in the range of 0 to 100.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.keywords

    This property holds a list of keywords describing the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.language

    This property holds the language of the media, as an ISO 639-2 code.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.publisher

    This property holds the publisher of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.copyright

    This property holds the media's copyright notice.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.parentalRating

    This property holds the parental rating of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.ratingOrganization

    This property holds the name of the rating organization responsible for the
    parental rating of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.size

    This property property holds the size of the media in bytes.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.mediaType

    This property holds the type of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.audioBitRate

    This property holds the bit rate of the media's audio stream in bits per
    second.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.audioCodec

    This property holds the encoding of the media audio stream.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.averageLevel

    This property holds the average volume level of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.channelCount

    This property holds the number of channels in the media's audio stream.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.peakValue

    This property holds the peak volume of media's audio stream.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.sampleRate

    This property holds the sample rate of the media's audio stream in hertz.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.albumTitle

    This property holds the title of the album the media belongs to.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.albumArtist

    This property holds the name of the principal artist of the album the media
    belongs to.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.contributingArtist

    This property holds the names of artists contributing to the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.composer

    This property holds the composer of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.conductor

    This property holds the conductor of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.lyrics

    This property holds the lyrics to the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.mood

    This property holds the mood of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.trackNumber

    This property holds the track number of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.trackCount

    This property holds the number of tracks on the album containing the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.coverArtUrlSmall

    This property holds the URL of a small cover art image.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.coverArtUrlLarge

    This property holds the URL of a large cover art image.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.resolution

    This property holds the dimension of an image or video.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.pixelAspectRatio

    This property holds the pixel aspect ratio of an image or video.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.videoFrameRate

    This property holds the frame rate of the media's video stream.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.videoBitRate

    This property holds the bit rate of the media's video stream in bits per
    second.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.videoCodec

    This property holds the encoding of the media's video stream.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.posterUrl

    This property holds the URL of a poster image.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.chapterNumber

    This property holds the chapter number of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.director

    This property holds the director of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.leadPerformer

    This property holds the lead performer in the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::Audio::metaData.writer

    This property holds the writer of the media.

    \sa {QMediaMetaData}
*/

///////////// MediaPlayer Docs /////////////

/*!
    \qmltype MediaPlayer
    \instantiates QDeclarativeAudio
    \brief Add media playback to a scene.

    \inqmlmodule QtMultimedia
    \ingroup multimedia_qml
    \ingroup multimedia_audio_qml
    \ingroup multimedia_video_qml

    MediaPlayer is part of the \b{QtMultimedia 5.0} module.

    \qml
    import QtQuick 2.0
    import QtMultimedia 5.0

    Text {
        text: "Click Me!";
        font.pointSize: 24;
        width: 150; height: 50;

        MediaPlayer {
            id: playMusic
            source: "music.wav"
        }
        MouseArea {
            id: playArea
            anchors.fill: parent
            onPressed:  { playMusic.play() }
        }
    }
    \endqml

    You can use MediaPlayer by itself to play audio content (like \l Audio),
    or you can use it in conjunction with a \l VideoOutput for rendering video.

    \qml
    import QtQuick 2.0
    import QtMultimedia 5.0

    Item {
        MediaPlayer {
            id: mediaplayer
            source: "groovy_video.mp4"
        }

        VideoOutput {
            anchors.fill: parent
            source: mediaplayer
        }

        MouseArea {
            id: playArea
            anchors.fill: parent
            onPressed: mediaplayer.play();
        }
    }
    \endqml

    \sa VideoOutput
*/

/*!
    \qmlproperty enumeration QtMultimedia::MediaPlayer::availability

    Returns the availability state of the media player.

    This is one of:
    \table
    \header \li Value \li Description
    \row \li Available
        \li The media player is available to use.
    \row \li Busy
        \li The media player is usually available, but some other
           process is utilizing the hardware necessary to play media.
    \row \li Unavailable
        \li There are no supported media playback facilities.
    \row \li ResourceMissing
        \li There is one or more resources missing, so the media player cannot
           be used.  It may be possible to try again at a later time.
    \endtable
 */

/*!
    \qmlmethod QtMultimedia::MediaPlayer::play()

    Starts playback of the media.

    Sets the \l playbackState property to PlayingState.
*/

/*!
    \qmlmethod QtMultimedia::MediaPlayer::pause()

    Pauses playback of the media.

    Sets the \l playbackState property to PausedState.
*/

/*!
    \qmlmethod QtMultimedia::MediaPlayer::stop()

    Stops playback of the media.

    Sets the \l playbackState property to StoppedState.
*/

/*!
    \qmlproperty url QtMultimedia::MediaPlayer::source

    This property holds the source URL of the media.
*/

/*!
    \qmlproperty int QtMultimedia::MediaPlayer::loops

    This property holds the number of times the media is played. A value of \c 0 or \c 1 means
    the media will be played only once; set to \c MediaPlayer.Infinite to enable infinite looping.

    The value can be changed while the media is playing, in which case it will update
    the remaining loops to the new value.

    The default is \c 1.
*/

/*!
    \qmlproperty bool QtMultimedia::MediaPlayer::autoLoad

    This property indicates if loading of media should begin immediately.

    Defaults to true, if false media will not be loaded until playback is started.
*/

/*!
    \qmlsignal QtMultimedia::MediaPlayer::playbackStateChanged()

    This signal is emitted when the \l playbackState property is altered.

    The corresponding handler is \c onPlaybackStateChanged.
*/


/*!
    \qmlsignal QtMultimedia::MediaPlayer::paused()

    This signal is emitted when playback is paused.

    The corresponding handler is \c onPaused.
*/

/*!
    \qmlsignal QtMultimedia::MediaPlayer::stopped()

    This signal is emitted when playback is stopped.

    The corresponding handler is \c onStopped.
*/

/*!
    \qmlsignal QtMultimedia::MediaPlayer::playing()

    This signal is emitted when playback is started or resumed.

    The corresponding handler is \c onPlaying.
*/

/*!
    \qmlproperty enumeration QtMultimedia::MediaPlayer::status

    This property holds the status of media loading. It can be one of:

    \list
    \li NoMedia - no media has been set.
    \li Loading - the media is currently being loaded.
    \li Loaded - the media has been loaded.
    \li Buffering - the media is buffering data.
    \li Stalled - playback has been interrupted while the media is buffering data.
    \li Buffered - the media has buffered data.
    \li EndOfMedia - the media has played to the end.
    \li InvalidMedia - the media cannot be played.
    \li UnknownStatus - the status of the media is unknown.
    \endlist
*/

/*!
    \qmlproperty enumeration QtMultimedia::MediaPlayer::playbackState

    This property holds the state of media playback. It can be one of:

    \list
    \li PlayingState - the media is currently playing.
    \li PausedState - playback of the media has been suspended.
    \li StoppedState - playback of the media is yet to begin.
    \endlist
*/

/*!
    \qmlproperty bool QtMultimedia::MediaPlayer::autoPlay

    This property controls whether the media will begin to play on start up.

    Defaults to \c false. If set to \c true, the value of autoLoad will be overwritten to \c true.
*/

/*!
    \qmlproperty int QtMultimedia::MediaPlayer::duration

    This property holds the duration of the media in milliseconds.

    If the media doesn't have a fixed duration (a live stream for example) this will be 0.
*/

/*!
    \qmlproperty int QtMultimedia::MediaPlayer::position

    This property holds the current playback position in milliseconds.

    To change this position, use the \l seek() method.

    \sa seek()
*/

/*!
    \qmlproperty real QtMultimedia::MediaPlayer::volume

    This property holds the volume of the audio output, from 0.0 (silent) to 1.0 (maximum volume).

    Defaults to 1.0.
*/

/*!
    \qmlproperty bool QtMultimedia::MediaPlayer::muted

    This property holds whether the audio output is muted.

    Defaults to false.
*/

/*!
    \qmlproperty bool QtMultimedia::MediaPlayer::hasAudio

    This property holds whether the media contains audio.
*/

/*!
    \qmlproperty bool QtMultimedia::MediaPlayer::hasVideo

    This property holds whether the media contains video.
*/

/*!
    \qmlproperty real QtMultimedia::MediaPlayer::bufferProgress

    This property holds how much of the data buffer is currently filled, from \c 0.0 (empty) to
    \c 1.0 (full).

    Playback can start or resume only when the buffer is entirely filled, in which case the
    status is \c MediaPlayer.Buffered or \c MediaPlayer.Buffering. A value lower than \c 1.0
    implies that the status is \c MediaPlayer.Stalled.

    \sa status
*/

/*!
    \qmlproperty bool QtMultimedia::MediaPlayer::seekable

    This property holds whether position of the audio can be changed.

    If true, calling the \l seek() method will cause playback to seek to the new position.
*/

/*!
    \qmlmethod QtMultimedia::MediaPlayer::seek(offset)

    If the \l seekable property is true, seeks the current
    playback position to \a offset.

    Seeking may be asynchronous, so the \l position property
    may not be updated immediately.

    \sa seekable, position
*/

/*!
    \qmlproperty real QtMultimedia::MediaPlayer::playbackRate

    This property holds the rate at which audio is played at as a multiple of the normal rate.

    Defaults to 1.0.
*/

/*!
    \qmlproperty enumeration QtMultimedia::MediaPlayer::error

    This property holds the error state of the audio.  It can be one of:

    \table
    \header \li Value \li Description
    \row \li NoError
        \li There is no current error.
    \row \li ResourceError
        \li The audio cannot be played due to a problem allocating resources.
    \row \li FormatError
        \li The audio format is not supported.
    \row \li NetworkError
        \li The audio cannot be played due to network issues.
    \row \li AccessDenied
        \li The audio cannot be played due to insufficient permissions.
    \row \li ServiceMissing
        \li The audio cannot be played because the media service could not be
    instantiated.
    \endtable
*/

/*!
    \qmlproperty string QtMultimedia::MediaPlayer::errorString

    This property holds a string describing the current error condition in more detail.
*/

/*!
    \qmlsignal QtMultimedia::MediaPlayer::error(error, errorString)

    This signal is emitted when an \l {QMediaPlayer::Error}{error} has
    occurred.  The errorString parameter may contain more detailed
    information about the error.

    The corresponding handler is \c onError.
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.title

    This property holds the title of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.subTitle

    This property holds the sub-title of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.author

    This property holds the author of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.comment

    This property holds a user comment about the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.description

    This property holds a description of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.category

    This property holds the category of the media

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.genre

    This property holds the genre of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.year

    This property holds the year of release of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.date

    This property holds the date of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.userRating

    This property holds a user rating of the media in the range of 0 to 100.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.keywords

    This property holds a list of keywords describing the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.language

    This property holds the language of the media, as an ISO 639-2 code.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.publisher

    This property holds the publisher of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.copyright

    This property holds the media's copyright notice.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.parentalRating

    This property holds the parental rating of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.ratingOrganization

    This property holds the name of the rating organization responsible for the
    parental rating of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.size

    This property property holds the size of the media in bytes.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.mediaType

    This property holds the type of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.audioBitRate

    This property holds the bit rate of the media's audio stream in bits per
    second.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.audioCodec

    This property holds the encoding of the media audio stream.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.averageLevel

    This property holds the average volume level of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.channelCount

    This property holds the number of channels in the media's audio stream.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.peakValue

    This property holds the peak volume of media's audio stream.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.sampleRate

    This property holds the sample rate of the media's audio stream in hertz.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.albumTitle

    This property holds the title of the album the media belongs to.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.albumArtist

    This property holds the name of the principal artist of the album the media
    belongs to.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.contributingArtist

    This property holds the names of artists contributing to the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.composer

    This property holds the composer of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.conductor

    This property holds the conductor of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.lyrics

    This property holds the lyrics to the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.mood

    This property holds the mood of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.trackNumber

    This property holds the track number of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.trackCount

    This property holds the number of tracks on the album containing the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.coverArtUrlSmall

    This property holds the URL of a small cover art image.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.coverArtUrlLarge

    This property holds the URL of a large cover art image.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.resolution

    This property holds the dimension of an image or video.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.pixelAspectRatio

    This property holds the pixel aspect ratio of an image or video.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.videoFrameRate

    This property holds the frame rate of the media's video stream.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.videoBitRate

    This property holds the bit rate of the media's video stream in bits per
    second.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.videoCodec

    This property holds the encoding of the media's video stream.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.posterUrl

    This property holds the URL of a poster image.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.chapterNumber

    This property holds the chapter number of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.director

    This property holds the director of the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.leadPerformer

    This property holds the lead performer in the media.

    \sa {QMediaMetaData}
*/

/*!
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.writer

    This property holds the writer of the media.

    \sa {QMediaMetaData}
*/

QT_END_NAMESPACE

#include "moc_qdeclarativeaudio_p.cpp"


