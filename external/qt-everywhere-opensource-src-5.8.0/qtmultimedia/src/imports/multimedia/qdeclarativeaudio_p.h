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

#ifndef QDECLARATIVEAUDIO_P_H
#define QDECLARATIVEAUDIO_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qbasictimer.h>
#include <QtQml/qqmlparserstatus.h>
#include <QtQml/qqml.h>
#include <QtQml/qjsvalue.h>

#include <qmediaplayer.h>

QT_BEGIN_NAMESPACE

class QTimerEvent;
class QMediaPlayerControl;
class QMediaService;
class QMediaServiceProvider;
class QMetaDataReaderControl;
class QDeclarativePlaylist;
class QDeclarativeMediaBaseAnimation;
class QDeclarativeMediaMetaData;
class QMediaAvailabilityControl;

class QDeclarativeAudio : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QDeclarativePlaylist *playlist READ playlist WRITE setPlaylist NOTIFY playlistChanged REVISION 1)
    Q_PROPERTY(int loops READ loopCount WRITE setLoopCount NOTIFY loopCountChanged)
    Q_PROPERTY(PlaybackState playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(bool autoPlay READ autoPlay WRITE setAutoPlay NOTIFY autoPlayChanged)
    Q_PROPERTY(bool autoLoad READ isAutoLoad WRITE setAutoLoad NOTIFY autoLoadChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(int position READ position NOTIFY positionChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ isMuted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(bool hasAudio READ hasAudio NOTIFY hasAudioChanged)
    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY hasVideoChanged)
    Q_PROPERTY(qreal bufferProgress READ bufferProgress NOTIFY bufferProgressChanged)
    Q_PROPERTY(bool seekable READ isSeekable NOTIFY seekableChanged)
    Q_PROPERTY(qreal playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)
    Q_PROPERTY(Error error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(QDeclarativeMediaMetaData *metaData READ metaData CONSTANT)
    Q_PROPERTY(QObject *mediaObject READ mediaObject NOTIFY mediaObjectChanged SCRIPTABLE false DESIGNABLE false)
    Q_PROPERTY(Availability availability READ availability NOTIFY availabilityChanged)
    Q_PROPERTY(AudioRole audioRole READ audioRole WRITE setAudioRole NOTIFY audioRoleChanged REVISION 1)
    Q_ENUMS(Status)
    Q_ENUMS(Error)
    Q_ENUMS(Loop)
    Q_ENUMS(PlaybackState)
    Q_ENUMS(Availability)
    Q_ENUMS(AudioRole)
    Q_INTERFACES(QQmlParserStatus)
public:
    enum Status
    {
        UnknownStatus = QMediaPlayer::UnknownMediaStatus,
        NoMedia       = QMediaPlayer::NoMedia,
        Loading       = QMediaPlayer::LoadingMedia,
        Loaded        = QMediaPlayer::LoadedMedia,
        Stalled       = QMediaPlayer::StalledMedia,
        Buffering     = QMediaPlayer::BufferingMedia,
        Buffered      = QMediaPlayer::BufferedMedia,
        EndOfMedia    = QMediaPlayer::EndOfMedia,
        InvalidMedia  = QMediaPlayer::InvalidMedia
    };

    enum Error
    {
        NoError        = QMediaPlayer::NoError,
        ResourceError  = QMediaPlayer::ResourceError,
        FormatError    = QMediaPlayer::FormatError,
        NetworkError   = QMediaPlayer::NetworkError,
        AccessDenied   = QMediaPlayer::AccessDeniedError,
        ServiceMissing = QMediaPlayer::ServiceMissingError
    };

    enum Loop
    {
        Infinite = -1
    };

    enum PlaybackState
    {
        PlayingState = QMediaPlayer::PlayingState,
        PausedState = QMediaPlayer::PausedState,
        StoppedState = QMediaPlayer::StoppedState
    };

    enum Availability {
        Available = QMultimedia::Available,
        Busy = QMultimedia::Busy,
        Unavailable = QMultimedia::ServiceMissing,
        ResourceMissing = QMultimedia::ResourceError
    };

    enum AudioRole {
        UnknownRole = QAudio::UnknownRole,
        AccessibilityRole = QAudio::AccessibilityRole,
        AlarmRole = QAudio::AlarmRole,
        GameRole = QAudio::GameRole,
        MusicRole = QAudio::MusicRole,
        NotificationRole = QAudio::NotificationRole,
        RingtoneRole = QAudio::RingtoneRole,
        SonificationRole = QAudio::SonificationRole,
        VideoRole = QAudio::VideoRole,
        VoiceCommunicationRole = QAudio::VoiceCommunicationRole
    };

    QDeclarativeAudio(QObject *parent = 0);
    ~QDeclarativeAudio();

    bool hasAudio() const;
    bool hasVideo() const;

    Status status() const;
    Error error() const;
    PlaybackState playbackState() const;
    void setPlaybackState(QMediaPlayer::State playbackState);

    void classBegin();
    void componentComplete();

    QObject *mediaObject() { return m_player; }

    Availability availability() const;

    AudioRole audioRole() const;
    void setAudioRole(AudioRole audioRole);

    QUrl source() const;
    void setSource(const QUrl &url);

    QDeclarativePlaylist *playlist() const;
    void setPlaylist(QDeclarativePlaylist *playlist);

    int loopCount() const;
    void setLoopCount(int loopCount);

    int duration() const;

    int position() const;

    qreal volume() const;
    void setVolume(qreal volume);

    bool isMuted() const;
    void setMuted(bool muted);

    qreal bufferProgress() const;

    bool isSeekable() const;

    qreal playbackRate() const;
    void setPlaybackRate(qreal rate);

    QString errorString() const;

    QDeclarativeMediaMetaData *metaData() const;

    bool isAutoLoad() const;
    void setAutoLoad(bool autoLoad);

    bool autoPlay() const;
    void setAutoPlay(bool autoplay);

public Q_SLOTS:
    void play();
    void pause();
    void stop();
    void seek(int position);

    Q_REVISION(1) QJSValue supportedAudioRoles() const;

Q_SIGNALS:
    Q_REVISION(1) void playlistChanged();

    void sourceChanged();
    void autoLoadChanged();
    void loopCountChanged();

    void playbackStateChanged();
    void autoPlayChanged();

    void paused();
    void stopped();
    void playing();

    void statusChanged();

    void durationChanged();
    void positionChanged();

    void volumeChanged();
    void mutedChanged();
    void hasAudioChanged();
    void hasVideoChanged();

    void bufferProgressChanged();

    void seekableChanged();
    void playbackRateChanged();

    Q_REVISION(1) void audioRoleChanged();

    void availabilityChanged(Availability availability);

    void errorChanged();
    void error(QDeclarativeAudio::Error error, const QString &errorString);

    void mediaObjectChanged();

private Q_SLOTS:
    void _q_error(QMediaPlayer::Error);
    void _q_availabilityChanged(QMultimedia::AvailabilityStatus);
    void _q_statusChanged();
    void _q_mediaChanged(const QMediaContent&);

private:
    Q_DISABLE_COPY(QDeclarativeAudio)

    QDeclarativePlaylist *m_playlist;
    bool m_autoPlay;
    bool m_autoLoad;
    bool m_loaded;
    bool m_muted;
    bool m_complete;
    bool m_emitPlaylistChanged;
    int m_loopCount;
    int m_runningCount;
    int m_position;
    qreal m_vol;
    qreal m_playbackRate;
    AudioRole m_audioRole;

    QMediaPlayer::State m_playbackState;
    QMediaPlayer::MediaStatus m_status;
    QMediaPlayer::Error m_error;
    QString m_errorString;
    QUrl m_source;
    QMediaContent m_content;

    QScopedPointer<QDeclarativeMediaMetaData> m_metaData;

    QMediaPlayer *m_player;

    friend class QDeclarativeMediaBaseAnimation;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QT_PREPEND_NAMESPACE(QDeclarativeAudio))

#endif
