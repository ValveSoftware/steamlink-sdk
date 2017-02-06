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
#ifndef MMRENDERERMEDIAPLAYERCONTROL_H
#define MMRENDERERMEDIAPLAYERCONTROL_H

#include "mmrenderermetadata.h"
#include <qmediaplayercontrol.h>
#include <QtCore/qabstractnativeeventfilter.h>
#include <QtCore/qpointer.h>
#include <QtCore/qtimer.h>

typedef struct mmr_connection mmr_connection_t;
typedef struct mmr_context mmr_context_t;
typedef struct mmrenderer_monitor mmrenderer_monitor_t;

QT_BEGIN_NAMESPACE

class MmRendererMetaDataReaderControl;
class MmRendererPlayerVideoRendererControl;
class MmRendererVideoWindowControl;

class MmRendererMediaPlayerControl : public QMediaPlayerControl, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit MmRendererMediaPlayerControl(QObject *parent = 0);

    QMediaPlayer::State state() const Q_DECL_OVERRIDE;

    QMediaPlayer::MediaStatus mediaStatus() const Q_DECL_OVERRIDE;

    qint64 duration() const Q_DECL_OVERRIDE;

    qint64 position() const Q_DECL_OVERRIDE;
    void setPosition(qint64 position) Q_DECL_OVERRIDE;

    int volume() const Q_DECL_OVERRIDE;
    void setVolume(int volume) Q_DECL_OVERRIDE;

    bool isMuted() const Q_DECL_OVERRIDE;
    void setMuted(bool muted) Q_DECL_OVERRIDE;

    int bufferStatus() const Q_DECL_OVERRIDE;

    bool isAudioAvailable() const Q_DECL_OVERRIDE;
    bool isVideoAvailable() const Q_DECL_OVERRIDE;

    bool isSeekable() const Q_DECL_OVERRIDE;

    QMediaTimeRange availablePlaybackRanges() const Q_DECL_OVERRIDE;

    qreal playbackRate() const Q_DECL_OVERRIDE;
    void setPlaybackRate(qreal rate) Q_DECL_OVERRIDE;

    QMediaContent media() const Q_DECL_OVERRIDE;
    const QIODevice *mediaStream() const Q_DECL_OVERRIDE;
    void setMedia(const QMediaContent &media, QIODevice *stream) Q_DECL_OVERRIDE;

    void play() Q_DECL_OVERRIDE;
    void pause() Q_DECL_OVERRIDE;
    void stop() Q_DECL_OVERRIDE;

    MmRendererPlayerVideoRendererControl *videoRendererControl() const;
    void setVideoRendererControl(MmRendererPlayerVideoRendererControl *videoControl);

    MmRendererVideoWindowControl *videoWindowControl() const;
    void setVideoWindowControl(MmRendererVideoWindowControl *videoControl);
    void setMetaDataReaderControl(MmRendererMetaDataReaderControl *metaDataReaderControl);

protected:
    virtual void startMonitoring(int contextId, const QString &contextName) = 0;
    virtual void stopMonitoring() = 0;

    QString contextName() const;
    void openConnection();
    void emitMmError(const QString &msg);
    void emitPError(const QString &msg);
    void setMmPosition(qint64 newPosition);
    void setMmBufferStatus(const QString &bufferStatus);
    void setMmBufferLevel(const QString &bufferLevel);
    void handleMmStopped();
    void handleMmStatusUpdate(qint64 position);

    // must be called from subclass dtors (calls virtual function stopMonitoring())
    void destroy();

private Q_SLOTS:
    void continueLoadMedia();

private:
    QByteArray resourcePathForUrl(const QUrl &url);
    void closeConnection();
    void attach();
    void detach();
    void updateMetaData();

    // All these set the specified value to the backend, but neither emit changed signals
    // nor change the member value.
    void setVolumeInternal(int newVolume);
    void setPlaybackRateInternal(qreal rate);
    void setPositionInternal(qint64 position);

    void setMediaStatus(QMediaPlayer::MediaStatus status);
    void setState(QMediaPlayer::State state);

    enum StopCommand { StopMmRenderer, IgnoreMmRenderer };
    void stopInternal(StopCommand stopCommand);

    QMediaContent m_media;
    mmr_connection_t *m_connection;
    mmr_context_t *m_context;
    QString m_contextName;
    int m_audioId;
    QMediaPlayer::State m_state;
    int m_volume;
    bool m_muted;
    qreal m_rate;
    QPointer<MmRendererPlayerVideoRendererControl> m_videoRendererControl;
    QPointer<MmRendererVideoWindowControl> m_videoWindowControl;
    QPointer<MmRendererMetaDataReaderControl> m_metaDataReaderControl;
    MmRendererMetaData m_metaData;
    int m_id;
    qint64 m_position;
    QMediaPlayer::MediaStatus m_mediaStatus;
    bool m_playAfterMediaLoaded;
    bool m_inputAttached;
    int m_stopEventsToIgnore;
    int m_bufferLevel;
    QTimer m_loadingTimer;
};

QT_END_NAMESPACE

#endif
