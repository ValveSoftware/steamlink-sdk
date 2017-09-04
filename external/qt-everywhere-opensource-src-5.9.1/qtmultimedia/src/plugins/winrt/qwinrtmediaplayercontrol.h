/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

#ifndef QWINRTMEDIAPLAYERCONTROL_H
#define QWINRTMEDIAPLAYERCONTROL_H

#include <QtMultimedia/QMediaPlayerControl>

struct IMFMediaEngineClassFactory;

QT_BEGIN_NAMESPACE

class QVideoRendererControl;

class QWinRTMediaPlayerControlPrivate;
class QWinRTMediaPlayerControl : public QMediaPlayerControl
{
    Q_OBJECT
public:
    QWinRTMediaPlayerControl(IMFMediaEngineClassFactory *factory, QObject *parent = 0);
    ~QWinRTMediaPlayerControl();

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

    QVideoRendererControl *videoRendererControl();

private:
    Q_INVOKABLE void finishRead();

    QScopedPointer<QWinRTMediaPlayerControlPrivate, QWinRTMediaPlayerControlPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QWinRTMediaPlayerControl)
};

QT_END_NAMESPACE

#endif // QWINRTMEDIAPLAYERCONTROL_H
