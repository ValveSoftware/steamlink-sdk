/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
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
#include "mmrenderermediaplayercontrol.h"
#include "mmrenderermetadatareadercontrol.h"
#include "mmrendererplayervideorenderercontrol.h"
#include "mmrendererutil.h"
#include "mmrenderervideowindowcontrol.h"
#include <QtCore/qabstracteventdispatcher.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/quuid.h>
#include <mm/renderer.h>

#include <errno.h>
#include <sys/strm.h>
#include <sys/stat.h>

QT_BEGIN_NAMESPACE

static int idCounter = 0;

MmRendererMediaPlayerControl::MmRendererMediaPlayerControl(QObject *parent)
    : QMediaPlayerControl(parent),
      m_connection(0),
      m_context(0),
      m_audioId(-1),
      m_state(QMediaPlayer::StoppedState),
      m_volume(100),
      m_muted(false),
      m_rate(1),
      m_id(-1),
      m_position(0),
      m_mediaStatus(QMediaPlayer::NoMedia),
      m_playAfterMediaLoaded(false),
      m_inputAttached(false),
      m_stopEventsToIgnore(0),
      m_bufferLevel(0)
{
    m_loadingTimer.setSingleShot(true);
    m_loadingTimer.setInterval(0);
    connect(&m_loadingTimer, SIGNAL(timeout()), this, SLOT(continueLoadMedia()));
    QCoreApplication::eventDispatcher()->installNativeEventFilter(this);
}

void MmRendererMediaPlayerControl::destroy()
{
    stop();
    detach();
    closeConnection();
    QCoreApplication::eventDispatcher()->removeNativeEventFilter(this);
}

void MmRendererMediaPlayerControl::openConnection()
{
    m_connection = mmr_connect(NULL);
    if (!m_connection) {
        emitPError("Unable to connect to the multimedia renderer");
        return;
    }

    m_id = idCounter++;
    m_contextName = QString("MmRendererMediaPlayerControl_%1_%2").arg(m_id)
                                                         .arg(QCoreApplication::applicationPid());
    m_context = mmr_context_create(m_connection, m_contextName.toLatin1(),
                                   0, S_IRWXU|S_IRWXG|S_IRWXO);
    if (!m_context) {
        emitPError("Unable to create context");
        closeConnection();
        return;
    }

    startMonitoring(m_id, m_contextName);
}

void MmRendererMediaPlayerControl::handleMmStatusUpdate(qint64 newPosition)
{
    // Prevent spurious position change events from overriding our own position, for example
    // when setting the position to 0 in stop().
    // Also, don't change the position while we're loading the media, as then play() would
    // set a wrong initial position.
    if (m_state != QMediaPlayer::PlayingState ||
        m_mediaStatus == QMediaPlayer::LoadingMedia ||
        m_mediaStatus == QMediaPlayer::NoMedia ||
        m_mediaStatus == QMediaPlayer::InvalidMedia)
        return;

    setMmPosition(newPosition);
}

void MmRendererMediaPlayerControl::handleMmStopped()
{
    // Only react to stop events that happen when the end of the stream is reached and
    // playback is stopped because of this.
    // Ignore other stop event sources, souch as calling mmr_stop() ourselves and
    // mmr_input_attach().
    if (m_stopEventsToIgnore > 0) {
        --m_stopEventsToIgnore;
    } else {
        setMediaStatus(QMediaPlayer::EndOfMedia);
        stopInternal(IgnoreMmRenderer);
    }
}

void MmRendererMediaPlayerControl::closeConnection()
{
    stopMonitoring();

    if (m_context) {
        mmr_context_destroy(m_context);
        m_context = 0;
        m_contextName.clear();
    }

    if (m_connection) {
        mmr_disconnect(m_connection);
        m_connection = 0;
    }
}

QByteArray MmRendererMediaPlayerControl::resourcePathForUrl(const QUrl &url)
{
    // If this is a local file, mmrenderer expects the file:// prefix and an absolute path.
    // We treat URLs without scheme as local files, most likely someone just forgot to set the
    // file:// prefix when constructing the URL.
    if (url.isLocalFile() || url.scheme().isEmpty()) {
        QString relativeFilePath;
        if (!url.scheme().isEmpty())
            relativeFilePath = url.toLocalFile();
        else
            relativeFilePath = url.path();
        const QFileInfo fileInfo(relativeFilePath);
        return QFile::encodeName(QStringLiteral("file://") + fileInfo.absoluteFilePath());

    // HTTP or similar URL
    } else {
        return url.toEncoded();
    }
}

void MmRendererMediaPlayerControl::attach()
{
    // Should only be called in detached state
    Q_ASSERT(m_audioId == -1 && !m_inputAttached);

    if (m_media.isNull() || !m_context) {
        setMediaStatus(QMediaPlayer::NoMedia);
        return;
    }

    if (m_videoRendererControl)
        m_videoRendererControl->attachDisplay(m_context);

    if (m_videoWindowControl)
        m_videoWindowControl->attachDisplay(m_context);

    const QByteArray defaultAudioDevice = qgetenv("QQNX_RENDERER_DEFAULT_AUDIO_SINK");
    m_audioId = mmr_output_attach(m_context, defaultAudioDevice.isEmpty() ? "audio:default" : defaultAudioDevice.constData(), "audio");
    if (m_audioId == -1) {
        emitMmError("mmr_output_attach() for audio failed");
        return;
    }

    const QByteArray resourcePath = resourcePathForUrl(m_media.canonicalUrl());
    if (resourcePath.isEmpty()) {
        detach();
        return;
    }

    if (mmr_input_attach(m_context, resourcePath.constData(), "track") != 0) {
        emitMmError(QStringLiteral("mmr_input_attach() failed for ") + QString(resourcePath));
        setMediaStatus(QMediaPlayer::InvalidMedia);
        detach();
        return;
    }

    // For whatever reason, the mmrenderer sends out a MMR_STOPPED event when calling
    // mmr_input_attach() above. Ignore it, as otherwise we'll trigger stopping right after we
    // started.
    m_stopEventsToIgnore++;

    m_inputAttached = true;
    setMediaStatus(QMediaPlayer::LoadedMedia);

    // mm-renderer has buffer properties "status" and "level"
    // QMediaPlayer's buffer status maps to mm-renderer's buffer level
    m_bufferLevel = 0;
    emit bufferStatusChanged(m_bufferLevel);
}

void MmRendererMediaPlayerControl::detach()
{
    if (m_context) {
        if (m_inputAttached) {
            mmr_input_detach(m_context);
            m_inputAttached = false;
        }
        if (m_videoRendererControl)
            m_videoRendererControl->detachDisplay();
        if (m_videoWindowControl)
            m_videoWindowControl->detachDisplay();
        if (m_audioId != -1 && m_context) {
            mmr_output_detach(m_context, m_audioId);
            m_audioId = -1;
        }
    }

    m_loadingTimer.stop();
}

QMediaPlayer::State MmRendererMediaPlayerControl::state() const
{
    return m_state;
}

QMediaPlayer::MediaStatus MmRendererMediaPlayerControl::mediaStatus() const
{
    return m_mediaStatus;
}

qint64 MmRendererMediaPlayerControl::duration() const
{
    return m_metaData.duration();
}

qint64 MmRendererMediaPlayerControl::position() const
{
    return m_position;
}

void MmRendererMediaPlayerControl::setPosition(qint64 position)
{
    if (m_position != position) {
        m_position = position;

        // Don't update in stopped state, it would not have any effect. Instead, the position is
        // updated in play().
        if (m_state != QMediaPlayer::StoppedState)
            setPositionInternal(m_position);

        emit positionChanged(m_position);
    }
}

int MmRendererMediaPlayerControl::volume() const
{
    return m_volume;
}

void MmRendererMediaPlayerControl::setVolumeInternal(int newVolume)
{
    if (!m_context)
        return;

    newVolume = qBound(0, newVolume, 100);
    if (m_audioId != -1) {
        strm_dict_t * dict = strm_dict_new();
        dict = strm_dict_set(dict, "volume", QString::number(newVolume).toLatin1());
        if (mmr_output_parameters(m_context, m_audioId, dict) != 0)
            emitMmError("mmr_output_parameters: Setting volume failed");
    }
}

void MmRendererMediaPlayerControl::setPlaybackRateInternal(qreal rate)
{
    if (!m_context)
        return;

    const int mmRate = rate * 1000;
    if (mmr_speed_set(m_context, mmRate) != 0)
        emitMmError("mmr_speed_set failed");
}

void MmRendererMediaPlayerControl::setPositionInternal(qint64 position)
{
    if (!m_context)
        return;

    if (m_metaData.isSeekable()) {
        if (mmr_seek(m_context, QString::number(position).toLatin1()) != 0)
            emitMmError("Seeking failed");
    }
}

void MmRendererMediaPlayerControl::setMediaStatus(QMediaPlayer::MediaStatus status)
{
    if (m_mediaStatus != status) {
        m_mediaStatus = status;
        emit mediaStatusChanged(m_mediaStatus);
    }
}

void MmRendererMediaPlayerControl::setState(QMediaPlayer::State state)
{
    if (m_state != state) {
        if (m_videoRendererControl) {
            if (state == QMediaPlayer::PausedState || state == QMediaPlayer::StoppedState) {
                m_videoRendererControl->pause();
            } else if ((state == QMediaPlayer::PlayingState)
                       && (m_state == QMediaPlayer::PausedState
                           || m_state == QMediaPlayer::StoppedState)) {
                m_videoRendererControl->resume();
            }
        }

        m_state = state;
        emit stateChanged(m_state);
    }
}

void MmRendererMediaPlayerControl::stopInternal(StopCommand stopCommand)
{
    setPosition(0);

    if (m_state != QMediaPlayer::StoppedState) {

        if (stopCommand == StopMmRenderer) {
            ++m_stopEventsToIgnore;
            mmr_stop(m_context);
        }

        setState(QMediaPlayer::StoppedState);
    }
}

void MmRendererMediaPlayerControl::setVolume(int volume)
{
    const int newVolume = qBound(0, volume, 100);
    if (m_volume != newVolume) {
        m_volume = newVolume;
        if (!m_muted)
            setVolumeInternal(m_volume);
        emit volumeChanged(m_volume);
    }
}

bool MmRendererMediaPlayerControl::isMuted() const
{
    return m_muted;
}

void MmRendererMediaPlayerControl::setMuted(bool muted)
{
    if (m_muted != muted) {
        m_muted = muted;
        setVolumeInternal(muted ? 0 : m_volume);
        emit mutedChanged(muted);
    }
}

int MmRendererMediaPlayerControl::bufferStatus() const
{
    // mm-renderer has buffer properties "status" and "level"
    // QMediaPlayer's buffer status maps to mm-renderer's buffer level
    return m_bufferLevel;
}

bool MmRendererMediaPlayerControl::isAudioAvailable() const
{
    return m_metaData.hasAudio();
}

bool MmRendererMediaPlayerControl::isVideoAvailable() const
{
    return m_metaData.hasVideo();
}

bool MmRendererMediaPlayerControl::isSeekable() const
{
    return m_metaData.isSeekable();
}

QMediaTimeRange MmRendererMediaPlayerControl::availablePlaybackRanges() const
{
    // We can't get this information from the mmrenderer API yet, so pretend we can seek everywhere
    return QMediaTimeRange(0, m_metaData.duration());
}

qreal MmRendererMediaPlayerControl::playbackRate() const
{
    return m_rate;
}

void MmRendererMediaPlayerControl::setPlaybackRate(qreal rate)
{
    if (m_rate != rate) {
        m_rate = rate;
        setPlaybackRateInternal(m_rate);
        emit playbackRateChanged(m_rate);
    }
}

QMediaContent MmRendererMediaPlayerControl::media() const
{
    return m_media;
}

const QIODevice *MmRendererMediaPlayerControl::mediaStream() const
{
    // Always 0, we don't support QIODevice streams
    return 0;
}

void MmRendererMediaPlayerControl::setMedia(const QMediaContent &media, QIODevice *stream)
{
    Q_UNUSED(stream); // not supported

    stop();
    detach();

    m_media = media;
    emit mediaChanged(m_media);

    // Slight hack: With MediaPlayer QtQuick elements that have autoPlay set to true, playback
    // would start before the QtQuick canvas is propagated to all elements, and therefore our
    // video output would not work. Therefore, delay actually playing the media a bit so that the
    // canvas is ready.
    // The mmrenderer doesn't allow to attach video outputs after playing has started, otherwise
    // this would be unnecessary.
    if (!m_media.isNull()) {
        setMediaStatus(QMediaPlayer::LoadingMedia);
        m_loadingTimer.start(); // singleshot timer to continueLoadMedia()
    } else {
        continueLoadMedia(); // still needed, as it will update the media status and clear metadata
    }
}

void MmRendererMediaPlayerControl::continueLoadMedia()
{
    attach();
    updateMetaData();
    if (m_playAfterMediaLoaded)
        play();
}

QString MmRendererMediaPlayerControl::contextName() const
{
    return m_contextName;
}

MmRendererVideoWindowControl *MmRendererMediaPlayerControl::videoWindowControl() const
{
    return m_videoWindowControl;
}

void MmRendererMediaPlayerControl::play()
{
    if (m_playAfterMediaLoaded)
        m_playAfterMediaLoaded = false;

    // No-op if we are already playing, except if we were called from continueLoadMedia(), in which
    // case m_playAfterMediaLoaded is true (hence the 'else').
    else if (m_state == QMediaPlayer::PlayingState)
        return;

    if (m_mediaStatus == QMediaPlayer::LoadingMedia) {

        // State changes are supposed to be synchronous
        setState(QMediaPlayer::PlayingState);

        // Defer playing to later, when the timer triggers continueLoadMedia()
        m_playAfterMediaLoaded = true;
        return;
    }

    // Un-pause the state when it is paused
    if (m_state == QMediaPlayer::PausedState) {
        setPlaybackRateInternal(m_rate);
        setState(QMediaPlayer::PlayingState);
        return;
    }

    if (m_media.isNull() || !m_connection || !m_context || m_audioId == -1) {
        setState(QMediaPlayer::StoppedState);
        return;
    }

    if (m_mediaStatus == QMediaPlayer::EndOfMedia)
        m_position = 0;

    setPositionInternal(m_position);
    setVolumeInternal(m_muted ? 0 : m_volume);
    setPlaybackRateInternal(m_rate);

    if (mmr_play(m_context) != 0) {
        setState(QMediaPlayer::StoppedState);
        emitMmError("mmr_play() failed");
        return;
    }

    m_stopEventsToIgnore = 0;    // once playing, stop events must be proccessed
    setState( QMediaPlayer::PlayingState);
}

void MmRendererMediaPlayerControl::pause()
{
    if (m_state == QMediaPlayer::PlayingState) {
        setPlaybackRateInternal(0);
        setState(QMediaPlayer::PausedState);
    }
}

void MmRendererMediaPlayerControl::stop()
{
    stopInternal(StopMmRenderer);
}

MmRendererPlayerVideoRendererControl *MmRendererMediaPlayerControl::videoRendererControl() const
{
    return m_videoRendererControl;
}

void MmRendererMediaPlayerControl::setVideoRendererControl(MmRendererPlayerVideoRendererControl *videoControl)
{
    m_videoRendererControl = videoControl;
}

void MmRendererMediaPlayerControl::setVideoWindowControl(MmRendererVideoWindowControl *videoControl)
{
    m_videoWindowControl = videoControl;
}

void MmRendererMediaPlayerControl::setMetaDataReaderControl(MmRendererMetaDataReaderControl *metaDataReaderControl)
{
    m_metaDataReaderControl = metaDataReaderControl;
}

void MmRendererMediaPlayerControl::setMmPosition(qint64 newPosition)
{
    if (newPosition != 0 && newPosition != m_position) {
        m_position = newPosition;
        emit positionChanged(m_position);
    }
}

void MmRendererMediaPlayerControl::setMmBufferStatus(const QString &bufferStatus)
{
    if (bufferStatus == QLatin1String("buffering"))
        setMediaStatus(QMediaPlayer::BufferingMedia);
    else if (bufferStatus == QLatin1String("playing"))
        setMediaStatus(QMediaPlayer::BufferedMedia);
    // ignore "idle" buffer status
}

void MmRendererMediaPlayerControl::setMmBufferLevel(const QString &bufferLevel)
{
    // buffer level has format level/capacity, e.g. "91319/124402"
    const int slashPos = bufferLevel.indexOf('/');
    if (slashPos != -1) {
        const int fill = bufferLevel.leftRef(slashPos).toInt();
        const int capacity = bufferLevel.midRef(slashPos + 1).toInt();
        if (capacity != 0) {
            m_bufferLevel = fill / static_cast<float>(capacity) * 100.0f;
            emit bufferStatusChanged(m_bufferLevel);
        }
    }
}

void MmRendererMediaPlayerControl::updateMetaData()
{
    if (m_mediaStatus == QMediaPlayer::LoadedMedia)
        m_metaData.parse(m_contextName);
    else
        m_metaData.clear();

    if (m_videoWindowControl)
        m_videoWindowControl->setMetaData(m_metaData);

    if (m_metaDataReaderControl)
        m_metaDataReaderControl->setMetaData(m_metaData);

    emit durationChanged(m_metaData.duration());
    emit audioAvailableChanged(m_metaData.hasAudio());
    emit videoAvailableChanged(m_metaData.hasVideo());
    emit availablePlaybackRangesChanged(availablePlaybackRanges());
    emit seekableChanged(m_metaData.isSeekable());
}

void MmRendererMediaPlayerControl::emitMmError(const QString &msg)
{
    int errorCode = MMR_ERROR_NONE;
    const QString errorMessage = mmErrorMessage(msg, m_context, &errorCode);
    qDebug() << errorMessage;
    emit error(errorCode, errorMessage);
}

void MmRendererMediaPlayerControl::emitPError(const QString &msg)
{
    const QString errorMessage = QString("%1: %2").arg(msg).arg(strerror(errno));
    qDebug() << errorMessage;
    emit error(errno, errorMessage);
}

QT_END_NAMESPACE
