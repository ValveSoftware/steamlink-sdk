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

#include "qopenslesaudioinput.h"

#include "qopenslesengine.h"
#include <qbuffer.h>
#include <private/qaudiohelpers_p.h>
#include <qdebug.h>

#ifdef ANDROID
#include <SLES/OpenSLES_AndroidConfiguration.h>
#endif

QT_BEGIN_NAMESPACE

#define NUM_BUFFERS 2
#define DEFAULT_PERIOD_TIME_MS 50
#define MINIMUM_PERIOD_TIME_MS 5

#ifdef ANDROID
static void bufferQueueCallback(SLAndroidSimpleBufferQueueItf, void *context)
#else
static void bufferQueueCallback(SLBufferQueueItf, void *context)
#endif
{
    // Process buffer in main thread
    QMetaObject::invokeMethod(reinterpret_cast<QOpenSLESAudioInput*>(context), "processBuffer");
}

QOpenSLESAudioInput::QOpenSLESAudioInput(const QByteArray &device)
    : m_device(device)
    , m_engine(QOpenSLESEngine::instance())
    , m_recorderObject(0)
    , m_recorder(0)
    , m_bufferQueue(0)
    , m_pullMode(true)
    , m_processedBytes(0)
    , m_audioSource(0)
    , m_bufferIODevice(0)
    , m_errorState(QAudio::NoError)
    , m_deviceState(QAudio::StoppedState)
    , m_lastNotifyTime(0)
    , m_volume(1.0)
    , m_bufferSize(0)
    , m_periodSize(0)
    , m_intervalTime(1000)
    , m_buffers(new QByteArray[NUM_BUFFERS])
    , m_currentBuffer(0)
{
#ifdef ANDROID
    if (qstrcmp(device, QT_ANDROID_PRESET_CAMCORDER) == 0)
        m_recorderPreset = SL_ANDROID_RECORDING_PRESET_CAMCORDER;
    else if (qstrcmp(device, QT_ANDROID_PRESET_VOICE_RECOGNITION) == 0)
        m_recorderPreset = SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION;
    else
        m_recorderPreset = SL_ANDROID_RECORDING_PRESET_GENERIC;
#endif
}

QOpenSLESAudioInput::~QOpenSLESAudioInput()
{
    if (m_recorderObject)
        (*m_recorderObject)->Destroy(m_recorderObject);
    delete[] m_buffers;
}

QAudio::Error QOpenSLESAudioInput::error() const
{
    return m_errorState;
}

QAudio::State QOpenSLESAudioInput::state() const
{
    return m_deviceState;
}

void QOpenSLESAudioInput::setFormat(const QAudioFormat &format)
{
    if (m_deviceState == QAudio::StoppedState)
        m_format = format;
}

QAudioFormat QOpenSLESAudioInput::format() const
{
    return m_format;
}

void QOpenSLESAudioInput::start(QIODevice *device)
{
    if (m_deviceState != QAudio::StoppedState)
        stopRecording();

    if (!m_pullMode && m_bufferIODevice) {
        m_bufferIODevice->close();
        delete m_bufferIODevice;
        m_bufferIODevice = 0;
    }

    m_pullMode = true;
    m_audioSource = device;

    if (startRecording()) {
        m_deviceState = QAudio::ActiveState;
    } else {
        m_deviceState = QAudio::StoppedState;
        Q_EMIT errorChanged(m_errorState);
    }

    Q_EMIT stateChanged(m_deviceState);
}

QIODevice *QOpenSLESAudioInput::start()
{
    if (m_deviceState != QAudio::StoppedState)
        stopRecording();

    m_audioSource = 0;

    if (!m_pullMode && m_bufferIODevice) {
        m_bufferIODevice->close();
        delete m_bufferIODevice;
    }

    m_pullMode = false;
    m_pushBuffer.clear();
    m_bufferIODevice = new QBuffer(&m_pushBuffer);
    m_bufferIODevice->open(QIODevice::ReadOnly);

    if (startRecording()) {
        m_deviceState = QAudio::IdleState;
    } else {
        m_deviceState = QAudio::StoppedState;
        Q_EMIT errorChanged(m_errorState);
        m_bufferIODevice->close();
        delete m_bufferIODevice;
        m_bufferIODevice = 0;
    }

    Q_EMIT stateChanged(m_deviceState);
    return m_bufferIODevice;
}

bool QOpenSLESAudioInput::startRecording()
{
    m_processedBytes = 0;
    m_clockStamp.restart();
    m_lastNotifyTime = 0;

    SLresult result;

    // configure audio source
    SLDataLocator_IODevice loc_dev = { SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
                                       SL_DEFAULTDEVICEID_AUDIOINPUT, NULL };
    SLDataSource audioSrc = { &loc_dev, NULL };

    // configure audio sink
#ifdef ANDROID
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                                      NUM_BUFFERS };
#else
    SLDataLocator_BufferQueue loc_bq = { SL_DATALOCATOR_BUFFERQUEUE, NUM_BUFFERS };
#endif

    SLDataFormat_PCM format_pcm = QOpenSLESEngine::audioFormatToSLFormatPCM(m_format);
    SLDataSink audioSnk = { &loc_bq, &format_pcm };

    // create audio recorder
    // (requires the RECORD_AUDIO permission)
#ifdef ANDROID
    const SLInterfaceID id[2] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION };
    const SLboolean req[2] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };
#else
    const SLInterfaceID id[1] = { SL_IID_BUFFERQUEUE };
    const SLboolean req[1] = { SL_BOOLEAN_TRUE };
#endif

    result = (*m_engine->slEngine())->CreateAudioRecorder(m_engine->slEngine(), &m_recorderObject,
                                                          &audioSrc, &audioSnk,
                                                          sizeof(req) / sizeof(SLboolean), id, req);
    if (result != SL_RESULT_SUCCESS) {
        m_errorState = QAudio::OpenError;
        return false;
    }

#ifdef ANDROID
    // configure recorder source
    SLAndroidConfigurationItf configItf;
    result = (*m_recorderObject)->GetInterface(m_recorderObject, SL_IID_ANDROIDCONFIGURATION,
                                               &configItf);
    if (result != SL_RESULT_SUCCESS) {
        m_errorState = QAudio::OpenError;
        return false;
    }

    result = (*configItf)->SetConfiguration(configItf, SL_ANDROID_KEY_RECORDING_PRESET,
                                            &m_recorderPreset, sizeof(SLuint32));

    SLuint32 presetValue = SL_ANDROID_RECORDING_PRESET_NONE;
    SLuint32 presetSize = 2*sizeof(SLuint32); // intentionally too big
    result = (*configItf)->GetConfiguration(configItf, SL_ANDROID_KEY_RECORDING_PRESET,
                                            &presetSize, (void*)&presetValue);

    if (result != SL_RESULT_SUCCESS || presetValue == SL_ANDROID_RECORDING_PRESET_NONE) {
        m_errorState = QAudio::OpenError;
        return false;
    }
#endif

    // realize the audio recorder
    result = (*m_recorderObject)->Realize(m_recorderObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        m_errorState = QAudio::OpenError;
        return false;
    }

    // get the record interface
    result = (*m_recorderObject)->GetInterface(m_recorderObject, SL_IID_RECORD, &m_recorder);
    if (result != SL_RESULT_SUCCESS) {
        m_errorState = QAudio::FatalError;
        return false;
    }

    // get the buffer queue interface
#ifdef ANDROID
    SLInterfaceID bufferqueueItfID = SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
#else
    SLInterfaceID bufferqueueItfID = SL_IID_BUFFERQUEUE;
#endif
    result = (*m_recorderObject)->GetInterface(m_recorderObject, bufferqueueItfID, &m_bufferQueue);
    if (result != SL_RESULT_SUCCESS) {
        m_errorState = QAudio::FatalError;
        return false;
    }

    // register callback on the buffer queue
    result = (*m_bufferQueue)->RegisterCallback(m_bufferQueue, bufferQueueCallback, this);
    if (result != SL_RESULT_SUCCESS) {
        m_errorState = QAudio::FatalError;
        return false;
    }

    if (m_bufferSize <= 0) {
        m_bufferSize = m_format.bytesForDuration(DEFAULT_PERIOD_TIME_MS * 1000);
    } else {
        int minimumBufSize = m_format.bytesForDuration(MINIMUM_PERIOD_TIME_MS * 1000);
        if (m_bufferSize < minimumBufSize)
            m_bufferSize = minimumBufSize;
    }

    m_periodSize = m_bufferSize;

    // enqueue empty buffers to be filled by the recorder
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        m_buffers[i].resize(m_periodSize);

        result = (*m_bufferQueue)->Enqueue(m_bufferQueue, m_buffers[i].data(), m_periodSize);
        if (result != SL_RESULT_SUCCESS) {
            m_errorState = QAudio::FatalError;
            return false;
        }
    }

    // start recording
    result = (*m_recorder)->SetRecordState(m_recorder, SL_RECORDSTATE_RECORDING);
    if (result != SL_RESULT_SUCCESS) {
        m_errorState = QAudio::FatalError;
        return false;
    }

    m_errorState = QAudio::NoError;

    return true;
}

void QOpenSLESAudioInput::stop()
{
    if (m_deviceState == QAudio::StoppedState)
        return;

    m_deviceState = QAudio::StoppedState;

    stopRecording();

    m_errorState = QAudio::NoError;
    Q_EMIT stateChanged(m_deviceState);
}

void QOpenSLESAudioInput::stopRecording()
{
    flushBuffers();

    (*m_recorder)->SetRecordState(m_recorder, SL_RECORDSTATE_STOPPED);
    (*m_bufferQueue)->Clear(m_bufferQueue);

    (*m_recorderObject)->Destroy(m_recorderObject);
    m_recorderObject = 0;

    for (int i = 0; i < NUM_BUFFERS; ++i)
        m_buffers[i].clear();
    m_currentBuffer = 0;

    if (!m_pullMode && m_bufferIODevice) {
        m_bufferIODevice->close();
        delete m_bufferIODevice;
        m_bufferIODevice = 0;
        m_pushBuffer.clear();
    }
}

void QOpenSLESAudioInput::suspend()
{
    if (m_deviceState == QAudio::ActiveState) {
        m_deviceState = QAudio::SuspendedState;
        emit stateChanged(m_deviceState);

        (*m_recorder)->SetRecordState(m_recorder, SL_RECORDSTATE_PAUSED);
    }
}

void QOpenSLESAudioInput::resume()
{
    if (m_deviceState == QAudio::SuspendedState || m_deviceState == QAudio::IdleState) {
        (*m_recorder)->SetRecordState(m_recorder, SL_RECORDSTATE_RECORDING);

        m_deviceState = QAudio::ActiveState;
        emit stateChanged(m_deviceState);
    }
}

void QOpenSLESAudioInput::processBuffer()
{
    if (m_deviceState == QAudio::StoppedState || m_deviceState == QAudio::SuspendedState)
        return;

    if (m_deviceState != QAudio::ActiveState) {
        m_errorState = QAudio::NoError;
        m_deviceState = QAudio::ActiveState;
        emit stateChanged(m_deviceState);
    }

    QByteArray *processedBuffer = &m_buffers[m_currentBuffer];
    writeDataToDevice(processedBuffer->constData(), processedBuffer->size());

    // Re-enqueue the buffer
    SLresult result = (*m_bufferQueue)->Enqueue(m_bufferQueue,
                                                processedBuffer->data(),
                                                processedBuffer->size());

    m_currentBuffer = (m_currentBuffer + 1) % NUM_BUFFERS;

    // If the buffer queue is empty (shouldn't happen), stop recording.
#ifdef ANDROID
    SLAndroidSimpleBufferQueueState state;
#else
    SLBufferQueueState state;
#endif
    result = (*m_bufferQueue)->GetState(m_bufferQueue, &state);
    if (result != SL_RESULT_SUCCESS || state.count == 0) {
        stop();
        m_errorState = QAudio::FatalError;
        Q_EMIT errorChanged(m_errorState);
    }
}

void QOpenSLESAudioInput::writeDataToDevice(const char *data, int size)
{
    m_processedBytes += size;

    QByteArray outData;

    // Apply volume
    if (m_volume < 1.0f) {
        outData.resize(size);
        QAudioHelperInternal::qMultiplySamples(m_volume, m_format, data, outData.data(), size);
    } else {
        outData.append(data, size);
    }

    if (m_pullMode) {
        // write buffer to the QIODevice
        if (m_audioSource->write(outData) < 0) {
            stop();
            m_errorState = QAudio::IOError;
            Q_EMIT errorChanged(m_errorState);
        }
    } else {
        // emits readyRead() so user will call read() on QIODevice to get some audio data
        if (m_bufferIODevice != 0) {
            m_pushBuffer.append(outData);
            Q_EMIT m_bufferIODevice->readyRead();
        }
    }

    // Send notify signal if needed
    qint64 processedMsecs = processedUSecs() / 1000;
    if (m_intervalTime && (processedMsecs - m_lastNotifyTime) >= m_intervalTime) {
        Q_EMIT notify();
        m_lastNotifyTime = processedMsecs;
    }
}

void QOpenSLESAudioInput::flushBuffers()
{
    SLmillisecond recorderPos;
    (*m_recorder)->GetPosition(m_recorder, &recorderPos);
    qint64 devicePos = processedUSecs();

    qint64 delta = recorderPos * 1000 - devicePos;

    if (delta > 0)
        writeDataToDevice(m_buffers[m_currentBuffer].constData(), m_format.bytesForDuration(delta));
}

int QOpenSLESAudioInput::bytesReady() const
{
    if (m_deviceState == QAudio::ActiveState || m_deviceState == QAudio::SuspendedState)
        return m_bufferIODevice ? m_bufferIODevice->bytesAvailable() : m_periodSize;

    return 0;
}

void QOpenSLESAudioInput::setBufferSize(int value)
{
    m_bufferSize = value;
}

int QOpenSLESAudioInput::bufferSize() const
{
    return m_bufferSize;
}

int QOpenSLESAudioInput::periodSize() const
{
    return m_periodSize;
}

void QOpenSLESAudioInput::setNotifyInterval(int ms)
{
    m_intervalTime = qMax(0, ms);
}

int QOpenSLESAudioInput::notifyInterval() const
{
    return m_intervalTime;
}

qint64 QOpenSLESAudioInput::processedUSecs() const
{
    return m_format.durationForBytes(m_processedBytes);
}

qint64 QOpenSLESAudioInput::elapsedUSecs() const
{
    if (m_deviceState == QAudio::StoppedState)
        return 0;

    return m_clockStamp.elapsed() * qint64(1000);
}

void QOpenSLESAudioInput::setVolume(qreal vol)
{
    m_volume = vol;
}

qreal QOpenSLESAudioInput::volume() const
{
    return m_volume;
}

void QOpenSLESAudioInput::reset()
{
    stop();
}

QT_END_NAMESPACE
