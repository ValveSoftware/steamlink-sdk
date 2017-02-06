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

#ifndef QNXAUDIOINPUT_H
#define QNXAUDIOINPUT_H

#include "qaudiosystem.h"

#include <QSocketNotifier>
#include <QIODevice>
#include <QTime>
#include <QTimer>

#include <sys/asoundlib.h>

QT_BEGIN_NAMESPACE

class QnxAudioInput : public QAbstractAudioInput
{
    Q_OBJECT

public:
    QnxAudioInput();
    ~QnxAudioInput();

    void start(QIODevice*) Q_DECL_OVERRIDE;
    QIODevice* start() Q_DECL_OVERRIDE;
    void stop() Q_DECL_OVERRIDE;
    void reset() Q_DECL_OVERRIDE;
    void suspend() Q_DECL_OVERRIDE;
    void resume() Q_DECL_OVERRIDE;
    int bytesReady() const Q_DECL_OVERRIDE;
    int periodSize() const Q_DECL_OVERRIDE;
    void setBufferSize(int ) Q_DECL_OVERRIDE;
    int bufferSize() const  Q_DECL_OVERRIDE;
    void setNotifyInterval(int ) Q_DECL_OVERRIDE;
    int notifyInterval() const Q_DECL_OVERRIDE;
    qint64 processedUSecs() const Q_DECL_OVERRIDE;
    qint64 elapsedUSecs() const Q_DECL_OVERRIDE;
    QAudio::Error error() const Q_DECL_OVERRIDE;
    QAudio::State state() const Q_DECL_OVERRIDE;
    void setFormat(const QAudioFormat&) Q_DECL_OVERRIDE;
    QAudioFormat format() const Q_DECL_OVERRIDE;
    void setVolume(qreal) Q_DECL_OVERRIDE;
    qreal volume() const Q_DECL_OVERRIDE;

private slots:
    void userFeed();
    bool deviceReady();

private:
    friend class InputPrivate;

    bool open();
    void close();
    qint64 read(char *data, qint64 len);
    void setError(QAudio::Error error);
    void setState(QAudio::State state);

    QTime m_timeStamp;
    QTime m_clockStamp;
    QAudioFormat m_format;

    QIODevice *m_audioSource;
    snd_pcm_t *m_pcmHandle;
    QSocketNotifier *m_pcmNotifier;

    QAudio::Error m_error;
    QAudio::State m_state;

    qint64 m_bytesRead;
    qint64 m_elapsedTimeOffset;
    qint64 m_totalTimeValue;

    qreal m_volume;

    int m_bytesAvailable;
    int m_bufferSize;
    int m_periodSize;
    int m_intervalTime;

    bool m_pullMode;
};

class InputPrivate : public QIODevice
{
    Q_OBJECT
public:
    InputPrivate(QnxAudioInput *audio);

    qint64 readData(char *data, qint64 len) Q_DECL_OVERRIDE;
    qint64 writeData(const char *data, qint64 len) Q_DECL_OVERRIDE;

    void trigger();

private:
    QnxAudioInput *m_audioDevice;
};

QT_END_NAMESPACE

#endif
