/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QWINRTMEDIAPLAYERCONTROL_H
#define QWINRTMEDIAPLAYERCONTROL_H

#include <QtMultimedia/QMediaPlayerControl>

struct IMFMediaEngineClassFactory;

QT_USE_NAMESPACE

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

#endif // QWINRTMEDIAPLAYERCONTROL_H
