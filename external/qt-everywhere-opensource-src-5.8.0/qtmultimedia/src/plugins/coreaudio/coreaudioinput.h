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
#ifndef IOSAUDIOINPUT_H
#define IOSAUDIOINPUT_H

#include <qaudiosystem.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <AudioToolbox/AudioToolbox.h>

#include <QtCore/QIODevice>
#include <QtCore/QWaitCondition>
#include <QtCore/QMutex>
#include <QtCore/QTimer>

QT_BEGIN_NAMESPACE

class CoreAudioRingBuffer;
class CoreAudioPacketFeeder;
class CoreAudioInputBuffer;
class CoreAudioInputDevice;

class CoreAudioBufferList
{
public:
    CoreAudioBufferList(AudioStreamBasicDescription const& streamFormat);
    CoreAudioBufferList(AudioStreamBasicDescription const& streamFormat, char *buffer, int bufferSize);
    CoreAudioBufferList(AudioStreamBasicDescription const& streamFormat, int framesToBuffer);

    ~CoreAudioBufferList();

    AudioBufferList* audioBufferList() const { return m_bufferList; }
    char *data(int buffer = 0) const;
    qint64 bufferSize(int buffer = 0) const;
    int frameCount(int buffer = 0) const;
    int packetCount(int buffer = 0) const;
    int packetSize() const;
    void reset();

private:
    bool m_owner;
    int m_dataSize;
    AudioStreamBasicDescription m_streamDescription;
    AudioBufferList *m_bufferList;
};

class CoreAudioPacketFeeder
{
public:
    CoreAudioPacketFeeder(CoreAudioBufferList *abl);

    bool feed(AudioBufferList& dst, UInt32& packetCount);
    bool empty() const;

private:
    UInt32 m_totalPackets;
    UInt32 m_position;
    CoreAudioBufferList *m_audioBufferList;
};

class CoreAudioInputBuffer : public QObject
{
    Q_OBJECT

public:
    CoreAudioInputBuffer(int bufferSize,
                        int maxPeriodSize,
                        AudioStreamBasicDescription const& inputFormat,
                        AudioStreamBasicDescription const& outputFormat,
                        QObject *parent);

    ~CoreAudioInputBuffer();

    qreal volume() const;
    void setVolume(qreal v);

    qint64 renderFromDevice(AudioUnit audioUnit,
                             AudioUnitRenderActionFlags *ioActionFlags,
                             const AudioTimeStamp *inTimeStamp,
                             UInt32 inBusNumber,
                             UInt32 inNumberFrames);

    qint64 readBytes(char *data, qint64 len);

    void setFlushDevice(QIODevice *device);

    void startFlushTimer();
    void stopFlushTimer();

    void flush(bool all = false);
    void reset();
    int available() const;
    int used() const;

signals:
    void readyRead();

private slots:
    void flushBuffer();

private:
    bool m_deviceError;
    int m_maxPeriodSize;
    int m_periodTime;
    QIODevice *m_device;
    QTimer *m_flushTimer;
    CoreAudioRingBuffer *m_buffer;
    CoreAudioBufferList *m_inputBufferList;
    AudioConverterRef m_audioConverter;
    AudioStreamBasicDescription m_inputFormat;
    AudioStreamBasicDescription m_outputFormat;
    QAudioFormat m_qFormat;
    qreal m_volume;

    const static OSStatus as_empty = 'qtem';

    // Converter callback
    static OSStatus converterCallback(AudioConverterRef inAudioConverter,
                                UInt32 *ioNumberDataPackets,
                                AudioBufferList *ioData,
                                AudioStreamPacketDescription **outDataPacketDescription,
                                void *inUserData);
};

class CoreAudioInputDevice : public QIODevice
{
    Q_OBJECT

public:
    CoreAudioInputDevice(CoreAudioInputBuffer *audioBuffer, QObject *parent);

    qint64 readData(char *data, qint64 len);
    qint64 writeData(const char *data, qint64 len);

    bool isSequential() const { return true; }

private:
    CoreAudioInputBuffer *m_audioBuffer;
};

class CoreAudioInput : public QAbstractAudioInput
{
    Q_OBJECT

public:
    CoreAudioInput(const QByteArray &device);
    ~CoreAudioInput();

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

private slots:
    void deviceStoppped();

private:
    enum {
        Running,
        Stopped
    };

    bool open();
    void close();

    void audioThreadStart();
    void audioThreadStop();

    void audioDeviceStop();
    void audioDeviceActive();
    void audioDeviceFull();
    void audioDeviceError();

    void startTimers();
    void stopTimers();

    // Input callback
    static OSStatus inputCallback(void *inRefCon,
                                    AudioUnitRenderActionFlags *ioActionFlags,
                                    const AudioTimeStamp *inTimeStamp,
                                    UInt32 inBusNumber,
                                    UInt32 inNumberFrames,
                                    AudioBufferList *ioData);

    QByteArray m_device;
    bool m_isOpen;
    int m_periodSizeBytes;
    int m_internalBufferSize;
    qint64 m_totalFrames;
    QAudioFormat m_audioFormat;
    QIODevice *m_audioIO;
    AudioUnit m_audioUnit;
#if defined(Q_OS_OSX)
    AudioDeviceID m_audioDeviceId;
#endif
    Float64 m_clockFrequency;
    UInt64 m_startTime;
    QAudio::Error m_errorCode;
    QAudio::State m_stateCode;
    CoreAudioInputBuffer *m_audioBuffer;
    QMutex m_mutex;
    QWaitCondition m_threadFinished;
    QAtomicInt m_audioThreadState;
    QTimer *m_intervalTimer;
    AudioStreamBasicDescription m_streamFormat;
    AudioStreamBasicDescription m_deviceFormat;
    QAbstractAudioDeviceInfo *m_audioDeviceInfo;
    qreal m_volume;
};

QT_END_NAMESPACE

#endif // IOSAUDIOINPUT_H
