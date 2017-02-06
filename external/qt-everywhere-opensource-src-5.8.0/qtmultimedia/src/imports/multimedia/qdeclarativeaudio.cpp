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
#include <QtQml/qqmlinfo.h>

#include "qdeclarativeaudio_p.h"

#include <qmediaplayercontrol.h>
#include <qmediaavailabilitycontrol.h>

#include <qmediaservice.h>
#include <private/qmediaserviceprovider_p.h>
#include <qmetadatareadercontrol.h>
#include <qmediaavailabilitycontrol.h>

#include "qdeclarativeplaylist_p.h"
#include "qdeclarativemediametadata_p.h"

#include <QTimerEvent>
#include <QtQml/qqmlengine.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Audio
    \instantiates QDeclarativeAudio
    \brief Add audio playback to a scene.

    \inqmlmodule QtMultimedia
    \ingroup multimedia_qml
    \ingroup multimedia_audio_qml

    \qml
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
    , m_playlist(0)
    , m_autoPlay(false)
    , m_autoLoad(true)
    , m_loaded(false)
    , m_muted(false)
    , m_complete(false)
    , m_emitPlaylistChanged(false)
    , m_loopCount(1)
    , m_runningCount(0)
    , m_position(0)
    , m_vol(1.0)
    , m_playbackRate(1.0)
    , m_audioRole(UnknownRole)
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

/*!
    \qmlproperty enumeration QtMultimedia::Audio::audioRole

    This property holds the role of the audio stream. It can be set to specify the type of audio
    being played, allowing the system to make appropriate decisions when it comes to volume,
    routing or post-processing.

    The audio role must be set before setting the source property.

    Supported values can be retrieved with supportedAudioRoles().

    The value can be one of:
    \list
    \li UnknownRole - the role is unknown or undefined.
    \li MusicRole - music.
    \li VideoRole - soundtrack from a movie or a video.
    \li VoiceCommunicationRole - voice communications, such as telephony.
    \li AlarmRole - alarm.
    \li NotificationRole - notification, such as an incoming e-mail or a chat request.
    \li RingtoneRole - ringtone.
    \li AccessibilityRole - for accessibility, such as with a screen reader.
    \li SonificationRole - sonification, such as with user interface sounds.
    \li GameRole - game audio.
    \endlist

    \since 5.6
*/
QDeclarativeAudio::AudioRole QDeclarativeAudio::audioRole() const
{
    return !m_complete ? m_audioRole : AudioRole(m_player->audioRole());
}

void QDeclarativeAudio::setAudioRole(QDeclarativeAudio::AudioRole audioRole)
{
    if (this->audioRole() == audioRole)
        return;

    if (m_complete) {
        m_player->setAudioRole(QAudio::Role(audioRole));
    } else {
        m_audioRole = audioRole;
        emit audioRoleChanged();
    }
}

/*!
    \qmlmethod list<int> QtMultimedia::Audio::supportedAudioRoles()

    Returns a list of supported audio roles.

    If setting the audio role is not supported, an empty list is returned.

    \since 5.6
    \sa audioRole
*/
QJSValue QDeclarativeAudio::supportedAudioRoles() const
{
    QJSEngine *engine = qmlEngine(this);

    if (!m_complete)
        return engine->newArray();

    QList<QAudio::Role> roles = m_player->supportedAudioRoles();
    int size = roles.size();

    QJSValue result = engine->newArray(size);
    for (int i = 0; i < size; ++i)
        result.setProperty(i, roles.at(i));

    return result;
}

QUrl QDeclarativeAudio::source() const
{
    return m_source;
}

QDeclarativePlaylist *QDeclarativeAudio::playlist() const
{
    return m_playlist;
}

void QDeclarativeAudio::setPlaylist(QDeclarativePlaylist *playlist)
{
    if (playlist == m_playlist && m_source.isEmpty())
        return;

    if (!m_source.isEmpty()) {
        m_source.clear();
        emit sourceChanged();
    }

    m_playlist = playlist;
    m_content = m_playlist ?
        QMediaContent(m_playlist->mediaPlaylist(), QUrl(), false) : QMediaContent();
    m_loaded = false;
    if (m_complete && (m_autoLoad || m_content.isNull() || m_autoPlay)) {
        if (m_error != QMediaPlayer::ServiceMissingError && m_error != QMediaPlayer::NoError) {
            m_error = QMediaPlayer::NoError;
            m_errorString = QString();

            emit errorChanged();
        }

        if (!playlist)
            m_emitPlaylistChanged = true;
        m_player->setMedia(m_content, 0);
        m_loaded = true;
    }
    else
        emit playlistChanged();

    if (m_autoPlay)
        m_player->play();
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
    if (url == m_source && m_playlist == NULL)
        return;

    if (m_playlist) {
        m_playlist = NULL;
        emit playlistChanged();
    }

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

    Setting the \l source property clears the current \l playlist, if any.
*/

/*!
    \qmlproperty Playlist QtMultimedia::Audio::playlist

    This property holds the playlist used by the media player.

    Setting the \l playlist property resets the \l source to an empty string.

    \since 5.6
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

    This property holds the audio volume.

    The volume is scaled linearly from \c 0.0 (silence) to \c 1.0 (full volume). Values outside this
    range will be clamped.

    The default volume is \c 1.0.

    UI volume controls should usually be scaled nonlinearly. For example, using a logarithmic scale
    will produce linear changes in perceived loudness, which is what a user would normally expect
    from a volume control. See \l {QtMultimedia::QtMultimedia::convertVolume()}{QtMultimedia.convertVolume()}
    for more details.
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
    connect(m_player, SIGNAL(mediaChanged(const QMediaContent&)),
            this, SLOT(_q_mediaChanged(const QMediaContent&)));
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
    connect(m_player, SIGNAL(audioRoleChanged(QAudio::Role)),
            this, SIGNAL(audioRoleChanged()));

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
    if (m_audioRole != UnknownRole)
        m_player->setAudioRole(QAudio::Role(m_audioRole));

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

void QDeclarativeAudio::_q_mediaChanged(const QMediaContent &media)
{
    if (!media.playlist() && !m_emitPlaylistChanged) {
        emit sourceChanged();
    } else {
        m_emitPlaylistChanged = false;
        emit playlistChanged();
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
    \qmlproperty variant QtMultimedia::Audio::mediaObject

    This property holds the native media object.

    It can be used to get a pointer to a QMediaPlayer object in order to integrate with C++ code.

    \code
        QObject *qmlAudio; // The QML Audio object
        QMediaPlayer *player = qvariant_cast<QMediaPlayer *>(qmlAudio->property("mediaObject"));
    \endcode

    \note This property is not accessible from QML.
*/

/*!
    \qmlpropertygroup QtMultimedia::Audio::metaData
    \qmlproperty variant QtMultimedia::Audio::metaData.title
    \qmlproperty variant QtMultimedia::Audio::metaData.subTitle
    \qmlproperty variant QtMultimedia::Audio::metaData.author
    \qmlproperty variant QtMultimedia::Audio::metaData.comment
    \qmlproperty variant QtMultimedia::Audio::metaData.description
    \qmlproperty variant QtMultimedia::Audio::metaData.category
    \qmlproperty variant QtMultimedia::Audio::metaData.genre
    \qmlproperty variant QtMultimedia::Audio::metaData.year
    \qmlproperty variant QtMultimedia::Audio::metaData.date
    \qmlproperty variant QtMultimedia::Audio::metaData.userRating
    \qmlproperty variant QtMultimedia::Audio::metaData.keywords
    \qmlproperty variant QtMultimedia::Audio::metaData.language
    \qmlproperty variant QtMultimedia::Audio::metaData.publisher
    \qmlproperty variant QtMultimedia::Audio::metaData.copyright
    \qmlproperty variant QtMultimedia::Audio::metaData.parentalRating
    \qmlproperty variant QtMultimedia::Audio::metaData.ratingOrganization
    \qmlproperty variant QtMultimedia::Audio::metaData.size
    \qmlproperty variant QtMultimedia::Audio::metaData.mediaType
    \qmlproperty variant QtMultimedia::Audio::metaData.audioBitRate
    \qmlproperty variant QtMultimedia::Audio::metaData.audioCodec
    \qmlproperty variant QtMultimedia::Audio::metaData.averageLevel
    \qmlproperty variant QtMultimedia::Audio::metaData.channelCount
    \qmlproperty variant QtMultimedia::Audio::metaData.peakValue
    \qmlproperty variant QtMultimedia::Audio::metaData.sampleRate
    \qmlproperty variant QtMultimedia::Audio::metaData.albumTitle
    \qmlproperty variant QtMultimedia::Audio::metaData.albumArtist
    \qmlproperty variant QtMultimedia::Audio::metaData.contributingArtist
    \qmlproperty variant QtMultimedia::Audio::metaData.composer
    \qmlproperty variant QtMultimedia::Audio::metaData.conductor
    \qmlproperty variant QtMultimedia::Audio::metaData.lyrics
    \qmlproperty variant QtMultimedia::Audio::metaData.mood
    \qmlproperty variant QtMultimedia::Audio::metaData.trackNumber
    \qmlproperty variant QtMultimedia::Audio::metaData.trackCount
    \qmlproperty variant QtMultimedia::Audio::metaData.coverArtUrlSmall
    \qmlproperty variant QtMultimedia::Audio::metaData.coverArtUrlLarge
    \qmlproperty variant QtMultimedia::Audio::metaData.resolution
    \qmlproperty variant QtMultimedia::Audio::metaData.pixelAspectRatio
    \qmlproperty variant QtMultimedia::Audio::metaData.videoFrameRate
    \qmlproperty variant QtMultimedia::Audio::metaData.videoBitRate
    \qmlproperty variant QtMultimedia::Audio::metaData.videoCodec
    \qmlproperty variant QtMultimedia::Audio::metaData.posterUrl
    \qmlproperty variant QtMultimedia::Audio::metaData.chapterNumber
    \qmlproperty variant QtMultimedia::Audio::metaData.director
    \qmlproperty variant QtMultimedia::Audio::metaData.leadPerformer
    \qmlproperty variant QtMultimedia::Audio::metaData.writer

    These properties hold the meta data for the current media.

    \list
    \li \c metaData.title - the title of the media.
    \li \c metaData.subTitle - the sub-title of the media.
    \li \c metaData.author - the author of the media.
    \li \c metaData.comment - a user comment about the media.
    \li \c metaData.description - a description of the media.
    \li \c metaData.category - the category of the media.
    \li \c metaData.genre - the genre of the media.
    \li \c metaData.year - the year of release of the media.
    \li \c metaData.date - the date of the media.
    \li \c metaData.userRating - a user rating of the media in the range of 0 to 100.
    \li \c metaData.keywords - a list of keywords describing the media.
    \li \c metaData.language - the language of the media, as an ISO 639-2 code.
    \li \c metaData.publisher - the publisher of the media.
    \li \c metaData.copyright - the media's copyright notice.
    \li \c metaData.parentalRating - the parental rating of the media.
    \li \c metaData.ratingOrganization - the name of the rating organization responsible for the
                                         parental rating of the media.
    \li \c metaData.size - the size of the media in bytes.
    \li \c metaData.mediaType - the type of the media.
    \li \c metaData.audioBitRate - the bit rate of the media's audio stream in bits per second.
    \li \c metaData.audioCodec - the encoding of the media audio stream.
    \li \c metaData.averageLevel - the average volume level of the media.
    \li \c metaData.channelCount - the number of channels in the media's audio stream.
    \li \c metaData.peakValue - the peak volume of media's audio stream.
    \li \c metaData.sampleRate - the sample rate of the media's audio stream in hertz.
    \li \c metaData.albumTitle - the title of the album the media belongs to.
    \li \c metaData.albumArtist - the name of the principal artist of the album the media
                                  belongs to.
    \li \c metaData.contributingArtist - the names of artists contributing to the media.
    \li \c metaData.composer - the composer of the media.
    \li \c metaData.conductor - the conductor of the media.
    \li \c metaData.lyrics - the lyrics to the media.
    \li \c metaData.mood - the mood of the media.
    \li \c metaData.trackNumber - the track number of the media.
    \li \c metaData.trackCount - the number of tracks on the album containing the media.
    \li \c metaData.coverArtUrlSmall - the URL of a small cover art image.
    \li \c metaData.coverArtUrlLarge - the URL of a large cover art image.
    \li \c metaData.resolution - the dimension of an image or video.
    \li \c metaData.pixelAspectRatio - the pixel aspect ratio of an image or video.
    \li \c metaData.videoFrameRate - the frame rate of the media's video stream.
    \li \c metaData.videoBitRate - the bit rate of the media's video stream in bits per second.
    \li \c metaData.videoCodec - the encoding of the media's video stream.
    \li \c metaData.posterUrl - the URL of a poster image.
    \li \c metaData.chapterNumber - the chapter number of the media.
    \li \c metaData.director - the director of the media.
    \li \c metaData.leadPerformer - the lead performer in the media.
    \li \c metaData.writer - the writer of the media.
    \endlist

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

    \qml
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
    \qmlproperty enumeration QtMultimedia::MediaPlayer::audioRole

    This property holds the role of the audio stream. It can be set to specify the type of audio
    being played, allowing the system to make appropriate decisions when it comes to volume,
    routing or post-processing.

    The audio role must be set before setting the source property.

    Supported values can be retrieved with supportedAudioRoles().

    The value can be one of:
    \list
    \li UnknownRole - the role is unknown or undefined.
    \li MusicRole - music.
    \li VideoRole - soundtrack from a movie or a video.
    \li VoiceCommunicationRole - voice communications, such as telephony.
    \li AlarmRole - alarm.
    \li NotificationRole - notification, such as an incoming e-mail or a chat request.
    \li RingtoneRole - ringtone.
    \li AccessibilityRole - for accessibility, such as with a screen reader.
    \li SonificationRole - sonification, such as with user interface sounds.
    \li GameRole - game audio.
    \endlist

    \since 5.6
*/

/*!
    \qmlmethod list<int> QtMultimedia::MediaPlayer::supportedAudioRoles()

    Returns a list of supported audio roles.

    If setting the audio role is not supported, an empty list is returned.

    \since 5.6
    \sa audioRole
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

    Setting the \l source property clears the current \l playlist, if any.
*/

/*!
    \qmlproperty Playlist QtMultimedia::MediaPlayer::playlist

    This property holds the playlist used by the media player.

    Setting the \l playlist property resets the \l source to an empty string.

    \since 5.6
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

    This property holds the audio volume of the media player.

    The volume is scaled linearly from \c 0.0 (silence) to \c 1.0 (full volume). Values outside this
    range will be clamped.

    The default volume is \c 1.0.

    UI volume controls should usually be scaled nonlinearly. For example, using a logarithmic scale
    will produce linear changes in perceived loudness, which is what a user would normally expect
    from a volume control. See \l {QtMultimedia::QtMultimedia::convertVolume()}{QtMultimedia.convertVolume()}
    for more details.
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
    \qmlproperty variant QtMultimedia::MediaPlayer::mediaObject

    This property holds the native media object.

    It can be used to get a pointer to a QMediaPlayer object in order to integrate with C++ code.

    \code
        QObject *qmlMediaPlayer; // The QML MediaPlayer object
        QMediaPlayer *player = qvariant_cast<QMediaPlayer *>(qmlMediaPlayer->property("mediaObject"));
    \endcode

    \note This property is not accessible from QML.
*/

/*!
    \qmlpropertygroup QtMultimedia::MediaPlayer::metaData
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.title
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.subTitle
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.author
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.comment
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.description
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.category
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.genre
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.year
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.date
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.userRating
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.keywords
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.language
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.publisher
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.copyright
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.parentalRating
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.ratingOrganization
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.size
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.mediaType
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.audioBitRate
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.audioCodec
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.averageLevel
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.channelCount
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.peakValue
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.sampleRate
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.albumTitle
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.albumArtist
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.contributingArtist
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.composer
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.conductor
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.lyrics
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.mood
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.trackNumber
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.trackCount
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.coverArtUrlSmall
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.coverArtUrlLarge
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.resolution
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.pixelAspectRatio
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.videoFrameRate
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.videoBitRate
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.videoCodec
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.posterUrl
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.chapterNumber
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.director
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.leadPerformer
    \qmlproperty variant QtMultimedia::MediaPlayer::metaData.writer

    These properties hold the meta data for the current media.

    \list
    \li \c metaData.title - the title of the media.
    \li \c metaData.subTitle - the sub-title of the media.
    \li \c metaData.author - the author of the media.
    \li \c metaData.comment - a user comment about the media.
    \li \c metaData.description - a description of the media.
    \li \c metaData.category - the category of the media.
    \li \c metaData.genre - the genre of the media.
    \li \c metaData.year - the year of release of the media.
    \li \c metaData.date - the date of the media.
    \li \c metaData.userRating - a user rating of the media in the range of 0 to 100.
    \li \c metaData.keywords - a list of keywords describing the media.
    \li \c metaData.language - the language of the media, as an ISO 639-2 code.
    \li \c metaData.publisher - the publisher of the media.
    \li \c metaData.copyright - the media's copyright notice.
    \li \c metaData.parentalRating - the parental rating of the media.
    \li \c metaData.ratingOrganization - the name of the rating organization responsible for the
                                         parental rating of the media.
    \li \c metaData.size - the size of the media in bytes.
    \li \c metaData.mediaType - the type of the media.
    \li \c metaData.audioBitRate - the bit rate of the media's audio stream in bits per second.
    \li \c metaData.audioCodec - the encoding of the media audio stream.
    \li \c metaData.averageLevel - the average volume level of the media.
    \li \c metaData.channelCount - the number of channels in the media's audio stream.
    \li \c metaData.peakValue - the peak volume of media's audio stream.
    \li \c metaData.sampleRate - the sample rate of the media's audio stream in hertz.
    \li \c metaData.albumTitle - the title of the album the media belongs to.
    \li \c metaData.albumArtist - the name of the principal artist of the album the media
                                  belongs to.
    \li \c metaData.contributingArtist - the names of artists contributing to the media.
    \li \c metaData.composer - the composer of the media.
    \li \c metaData.conductor - the conductor of the media.
    \li \c metaData.lyrics - the lyrics to the media.
    \li \c metaData.mood - the mood of the media.
    \li \c metaData.trackNumber - the track number of the media.
    \li \c metaData.trackCount - the number of tracks on the album containing the media.
    \li \c metaData.coverArtUrlSmall - the URL of a small cover art image.
    \li \c metaData.coverArtUrlLarge - the URL of a large cover art image.
    \li \c metaData.resolution - the dimension of an image or video.
    \li \c metaData.pixelAspectRatio - the pixel aspect ratio of an image or video.
    \li \c metaData.videoFrameRate - the frame rate of the media's video stream.
    \li \c metaData.videoBitRate - the bit rate of the media's video stream in bits per second.
    \li \c metaData.videoCodec - the encoding of the media's video stream.
    \li \c metaData.posterUrl - the URL of a poster image.
    \li \c metaData.chapterNumber - the chapter number of the media.
    \li \c metaData.director - the director of the media.
    \li \c metaData.leadPerformer - the lead performer in the media.
    \li \c metaData.writer - the writer of the media.
    \endlist

    \sa {QMediaMetaData}
*/

QT_END_NAMESPACE

#include "moc_qdeclarativeaudio_p.cpp"


