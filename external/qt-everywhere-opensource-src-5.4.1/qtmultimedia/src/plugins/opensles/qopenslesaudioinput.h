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

#ifndef QOPENSLESAUDIOINPUT_H
#define QOPENSLESAUDIOINPUT_H

#include <qaudiosystem.h>
#include <QTime>
#include <SLES/OpenSLES.h>

#ifdef ANDROID
#include <SLES/OpenSLES_Android.h>

#define QT_ANDROID_PRESET_MIC "mic"
#define QT_ANDROID_PRESET_CAMCORDER "camcorder"
#define QT_ANDROID_PRESET_VOICE_RECOGNITION "voicerecognition"

#endif

QT_BEGIN_NAMESPACE

class QOpenSLESEngine;
class QIODevice;
class QBuffer;

class QOpenSLESAudioInput : public QAbstractAudioInput
{
    Q_OBJECT

public:
    QOpenSLESAudioInput(const QByteArray &device);
    ~QOpenSLESAudioInput();

    void start(QIODevice *device);
    QIODevice *start();
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
    void setFormat(const QAudioFormat &format);
    QAudioFormat format() const;

    void setVolume(qreal volume);
    qreal volume() const;

public Q_SLOTS:
    void processBuffer();

private:
    bool startRecording();
    void stopRecording();
    void writeDataToDevice(const char *data, int size);
    void flushBuffers();

    QByteArray m_device;
    QOpenSLESEngine *m_engine;
    SLObjectItf m_recorderObject;
    SLRecordItf m_recorder;
#ifdef ANDROID
    SLuint32 m_recorderPreset;
    SLAndroidSimpleBufferQueueItf m_bufferQueue;
#else
    SLBufferQueueItf m_bufferQueue;
#endif

    bool m_pullMode;
    qint64 m_processedBytes;
    QIODevice *m_audioSource;
    QBuffer *m_bufferIODevice;
    QByteArray m_pushBuffer;
    QAudioFormat m_format;
    QAudio::Error m_errorState;
    QAudio::State m_deviceState;
    QTime m_clockStamp;
    qint64 m_lastNotifyTime;
    qreal m_volume;
    int m_bufferSize;
    int m_periodSize;
    int m_intervalTime;
    QByteArray *m_buffers;
    int m_currentBuffer;
};

QT_END_NAMESPACE

#endif // QOPENSLESAUDIOINPUT_H
