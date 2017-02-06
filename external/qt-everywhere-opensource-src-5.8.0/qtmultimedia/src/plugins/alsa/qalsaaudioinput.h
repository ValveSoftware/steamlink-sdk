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


#ifndef QAUDIOINPUTALSA_H
#define QAUDIOINPUTALSA_H

#include <alsa/asoundlib.h>

#include <QtCore/qfile.h>
#include <QtCore/qdebug.h>
#include <QtCore/qtimer.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qiodevice.h>

#include <QtMultimedia/qaudio.h>
#include <QtMultimedia/qaudiodeviceinfo.h>
#include <QtMultimedia/qaudiosystem.h>

QT_BEGIN_NAMESPACE


class AlsaInputPrivate;

class RingBuffer
{
public:
    RingBuffer();

    void resize(int size);

    int bytesOfDataInBuffer() const;
    int freeBytes() const;

    const char *availableData() const;
    int availableDataBlockSize() const;
    void readBytes(int bytes);

    void write(char *data, int len);

private:
    int m_head;
    int m_tail;

    QByteArray m_data;
};

class QAlsaAudioInput : public QAbstractAudioInput
{
    Q_OBJECT
public:
    QAlsaAudioInput(const QByteArray &device);
    ~QAlsaAudioInput();

    qint64 read(char* data, qint64 len);

    void start(QIODevice* device);
    QIODevice* start();
    void stop();
    void reset();
    void suspend();
    void resume();
    int bytesReady() const;
    int periodSize() const;
    void setBufferSize(int value);
    int bufferSize() const;
    void setNotifyInterval(int milliSeconds);
    int notifyInterval() const;
    qint64 processedUSecs() const;
    qint64 elapsedUSecs() const;
    QAudio::Error error() const;
    QAudio::State state() const;
    void setFormat(const QAudioFormat& fmt);
    QAudioFormat format() const;
    void setVolume(qreal);
    qreal volume() const;
    bool resuming;
    snd_pcm_t* handle;
    qint64 totalTimeValue;
    QIODevice* audioSource;
    QAudioFormat settings;
    QAudio::Error errorState;
    QAudio::State deviceState;

private slots:
    void userFeed();
    bool deviceReady();

private:
    int checkBytesReady();
    int xrun_recovery(int err);
    int setFormat();
    bool open();
    void close();
    void drain();

    QTimer* timer;
    QTime timeStamp;
    QTime clockStamp;
    qint64 elapsedTimeOffset;
    int intervalTime;
    RingBuffer ringBuffer;
    int bytesAvailable;
    QByteArray m_device;
    bool pullMode;
    int buffer_size;
    int period_size;
    unsigned int buffer_time;
    unsigned int period_time;
    snd_pcm_uframes_t buffer_frames;
    snd_pcm_uframes_t period_frames;
    snd_pcm_access_t access;
    snd_pcm_format_t pcmformat;
    snd_pcm_hw_params_t *hwparams;
    qreal m_volume;
};

class AlsaInputPrivate : public QIODevice
{
    Q_OBJECT
public:
    AlsaInputPrivate(QAlsaAudioInput* audio);
    ~AlsaInputPrivate();

    qint64 readData( char* data, qint64 len);
    qint64 writeData(const char* data, qint64 len);

    void trigger();
private:
    QAlsaAudioInput *audioDevice;
};

QT_END_NAMESPACE


#endif
