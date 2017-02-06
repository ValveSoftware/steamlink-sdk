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
#include "coreaudiooutput.h"
#include "coreaudiosessionmanager.h"
#include "coreaudiodeviceinfo.h"
#include "coreaudioutils.h"

#include <QtCore/QDataStream>
#include <QtCore/QTimer>
#include <QtCore/QDebug>

#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#if defined(Q_OS_OSX)
# include <AudioUnit/AudioComponent.h>
#endif

#if defined(Q_OS_IOS) || defined(Q_OS_TVOS)
# include <QtMultimedia/private/qaudiohelpers_p.h>
#endif

QT_BEGIN_NAMESPACE

static const int DEFAULT_BUFFER_SIZE = 8 * 1024;

CoreAudioOutputBuffer::CoreAudioOutputBuffer(int bufferSize, int maxPeriodSize, const QAudioFormat &audioFormat)
    : m_deviceError(false)
    , m_maxPeriodSize(maxPeriodSize)
    , m_device(0)
{
    m_buffer = new CoreAudioRingBuffer(bufferSize + (bufferSize % maxPeriodSize == 0 ? 0 : maxPeriodSize - (bufferSize % maxPeriodSize)));
    m_bytesPerFrame = (audioFormat.sampleSize() / 8) * audioFormat.channelCount();
    m_periodTime = m_maxPeriodSize / m_bytesPerFrame * 1000 / audioFormat.sampleRate();

    m_fillTimer = new QTimer(this);
    connect(m_fillTimer, SIGNAL(timeout()), SLOT(fillBuffer()));
}

CoreAudioOutputBuffer::~CoreAudioOutputBuffer()
{
    delete m_buffer;
}

qint64 CoreAudioOutputBuffer::readFrames(char *data, qint64 maxFrames)
{
    bool    wecan = true;
    qint64  framesRead = 0;

    while (wecan && framesRead < maxFrames) {
        CoreAudioRingBuffer::Region region = m_buffer->acquireReadRegion((maxFrames - framesRead) * m_bytesPerFrame);

        if (region.second > 0) {
            // Ensure that we only read whole frames.
            region.second -= region.second % m_bytesPerFrame;

            if (region.second > 0) {
                memcpy(data + (framesRead * m_bytesPerFrame), region.first, region.second);
                framesRead += region.second / m_bytesPerFrame;
            } else
                wecan = false; // If there is only a partial frame left we should exit.
        }
        else
            wecan = false;

        m_buffer->releaseReadRegion(region);
    }

    if (framesRead == 0 && m_deviceError)
        framesRead = -1;

    return framesRead;
}

qint64 CoreAudioOutputBuffer::writeBytes(const char *data, qint64 maxSize)
{
    bool    wecan = true;
    qint64  bytesWritten = 0;

    maxSize -= maxSize % m_bytesPerFrame;
    while (wecan && bytesWritten < maxSize) {
        CoreAudioRingBuffer::Region region = m_buffer->acquireWriteRegion(maxSize - bytesWritten);

        if (region.second > 0) {
            memcpy(region.first, data + bytesWritten, region.second);
            bytesWritten += region.second;
        }
        else
            wecan = false;

        m_buffer->releaseWriteRegion(region);
    }

    if (bytesWritten > 0)
        emit readyRead();

    return bytesWritten;
}

int CoreAudioOutputBuffer::available() const
{
    return m_buffer->free();
}

void CoreAudioOutputBuffer::reset()
{
    m_buffer->reset();
    m_device = 0;
    m_deviceError = false;
}

void CoreAudioOutputBuffer::setPrefetchDevice(QIODevice *device)
{
    if (m_device != device) {
        m_device = device;
        if (m_device != 0)
            fillBuffer();
    }
}

void CoreAudioOutputBuffer::startFillTimer()
{
    if (m_device != 0)
        m_fillTimer->start(m_buffer->size() / 2 / m_maxPeriodSize * m_periodTime);
}

void CoreAudioOutputBuffer::stopFillTimer()
{
    m_fillTimer->stop();
}

void CoreAudioOutputBuffer::fillBuffer()
{
    const int free = m_buffer->free();
    const int writeSize = free - (free % m_maxPeriodSize);

    if (writeSize > 0) {
        bool    wecan = true;
        int     filled = 0;

        while (!m_deviceError && wecan && filled < writeSize) {
            CoreAudioRingBuffer::Region region = m_buffer->acquireWriteRegion(writeSize - filled);

            if (region.second > 0) {
                region.second = m_device->read(region.first, region.second);
                if (region.second > 0)
                    filled += region.second;
                else if (region.second == 0)
                    wecan = false;
                else if (region.second < 0) {
                    m_fillTimer->stop();
                    region.second = 0;
                    m_deviceError = true;
                }
            }
            else
                wecan = false;

            m_buffer->releaseWriteRegion(region);
        }

        if (filled > 0)
            emit readyRead();
    }
}

CoreAudioOutputDevice::CoreAudioOutputDevice(CoreAudioOutputBuffer *audioBuffer, QObject *parent)
    : QIODevice(parent)
    , m_audioBuffer(audioBuffer)
{
    open(QIODevice::WriteOnly | QIODevice::Unbuffered);
}

qint64 CoreAudioOutputDevice::readData(char *data, qint64 len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);

    return 0;
}

qint64 CoreAudioOutputDevice::writeData(const char *data, qint64 len)
{
    return m_audioBuffer->writeBytes(data, len);
}

CoreAudioOutput::CoreAudioOutput(const QByteArray &device)
    : m_isOpen(false)
    , m_internalBufferSize(DEFAULT_BUFFER_SIZE)
    , m_totalFrames(0)
    , m_audioIO(0)
    , m_audioUnit(0)
    , m_startTime(0)
    , m_audioBuffer(0)
    , m_cachedVolume(1.0)
    , m_pullMode(false)
    , m_errorCode(QAudio::NoError)
    , m_stateCode(QAudio::StoppedState)
{
#if defined(Q_OS_OSX)
    quint32 deviceID;
    QDataStream dataStream(device);
    dataStream >> deviceID >> m_device;
    m_audioDeviceId = AudioDeviceID(deviceID);
#else //iOS
    m_device = device;
#endif

    m_clockFrequency = CoreAudioUtils::frequency() / 1000;
    m_audioDeviceInfo = new CoreAudioDeviceInfo(device, QAudio::AudioOutput);
    m_audioThreadState.store(Stopped);

    m_intervalTimer = new QTimer(this);
    m_intervalTimer->setInterval(1000);
    connect(m_intervalTimer, SIGNAL(timeout()), this, SIGNAL(notify()));
}

CoreAudioOutput::~CoreAudioOutput()
{
    close();
    delete m_audioDeviceInfo;
}

void CoreAudioOutput::start(QIODevice *device)
{
    QIODevice* op = device;

    if (!m_audioDeviceInfo->isFormatSupported(m_audioFormat) || !open()) {
        m_stateCode = QAudio::StoppedState;
        m_errorCode = QAudio::OpenError;
        return;
    }

    reset();
    m_audioBuffer->reset();
    m_audioBuffer->setPrefetchDevice(op);

    if (op == 0) {
        op = m_audioIO;
        m_stateCode = QAudio::IdleState;
    }
    else
        m_stateCode = QAudio::ActiveState;

    // Start
    m_pullMode = true;
    m_errorCode = QAudio::NoError;
    m_totalFrames = 0;
    m_startTime = CoreAudioUtils::currentTime();

    if (m_stateCode == QAudio::ActiveState)
        audioThreadStart();

    emit stateChanged(m_stateCode);
}

QIODevice *CoreAudioOutput::start()
{
    if (!m_audioDeviceInfo->isFormatSupported(m_audioFormat) || !open()) {
        m_stateCode = QAudio::StoppedState;
        m_errorCode = QAudio::OpenError;
        return m_audioIO;
    }

    reset();
    m_audioBuffer->reset();
    m_audioBuffer->setPrefetchDevice(0);

    m_stateCode = QAudio::IdleState;

    // Start
    m_pullMode = false;
    m_errorCode = QAudio::NoError;
    m_totalFrames = 0;
    m_startTime = CoreAudioUtils::currentTime();

    emit stateChanged(m_stateCode);

    return m_audioIO;
}

void CoreAudioOutput::stop()
{
    QMutexLocker lock(&m_mutex);
    if (m_stateCode != QAudio::StoppedState) {
        audioThreadDrain();

        m_stateCode = QAudio::StoppedState;
        m_errorCode = QAudio::NoError;
        emit stateChanged(m_stateCode);
    }
}

void CoreAudioOutput::reset()
{
    QMutexLocker lock(&m_mutex);
    if (m_stateCode != QAudio::StoppedState) {
        audioThreadStop();

        m_stateCode = QAudio::StoppedState;
        m_errorCode = QAudio::NoError;
        emit stateChanged(m_stateCode);
    }
}

void CoreAudioOutput::suspend()
{
    QMutexLocker lock(&m_mutex);
    if (m_stateCode == QAudio::ActiveState || m_stateCode == QAudio::IdleState) {
        audioThreadStop();

        m_stateCode = QAudio::SuspendedState;
        m_errorCode = QAudio::NoError;
        emit stateChanged(m_stateCode);
    }
}

void CoreAudioOutput::resume()
{
    QMutexLocker lock(&m_mutex);
    if (m_stateCode == QAudio::SuspendedState) {
        audioThreadStart();

        m_stateCode = m_pullMode ? QAudio::ActiveState : QAudio::IdleState;
        m_errorCode = QAudio::NoError;
        emit stateChanged(m_stateCode);
    }
}

int CoreAudioOutput::bytesFree() const
{
    return m_audioBuffer->available();
}

int CoreAudioOutput::periodSize() const
{
    return m_periodSizeBytes;
}

void CoreAudioOutput::setBufferSize(int value)
{
    if (m_stateCode == QAudio::StoppedState)
        m_internalBufferSize = value;
}

int CoreAudioOutput::bufferSize() const
{
    return m_internalBufferSize;
}

void CoreAudioOutput::setNotifyInterval(int milliSeconds)
{
    if (m_intervalTimer->interval() == milliSeconds)
        return;

    if (milliSeconds <= 0)
        milliSeconds = 0;

    m_intervalTimer->setInterval(milliSeconds);
}

int CoreAudioOutput::notifyInterval() const
{
    return m_intervalTimer->interval();
}

qint64 CoreAudioOutput::processedUSecs() const
{
    return m_totalFrames * 1000000 / m_audioFormat.sampleRate();
}

qint64 CoreAudioOutput::elapsedUSecs() const
{
    if (m_stateCode == QAudio::StoppedState)
        return 0;

    return (CoreAudioUtils::currentTime() - m_startTime) / (m_clockFrequency / 1000);
}

QAudio::Error CoreAudioOutput::error() const
{
    return m_errorCode;
}

QAudio::State CoreAudioOutput::state() const
{
    return m_stateCode;
}

void CoreAudioOutput::setFormat(const QAudioFormat &format)
{
    if (m_stateCode == QAudio::StoppedState)
        m_audioFormat = format;
}

QAudioFormat CoreAudioOutput::format() const
{
    return m_audioFormat;
}

void CoreAudioOutput::setVolume(qreal volume)
{
    const qreal normalizedVolume = qBound(qreal(0.0), volume, qreal(1.0));
    if (!m_isOpen) {
        m_cachedVolume = normalizedVolume;
        return;
    }

#if defined(Q_OS_OSX)
    //on OS X the volume can be set directly on the AudioUnit
    if (AudioUnitSetParameter(m_audioUnit,
                              kHALOutputParam_Volume,
                              kAudioUnitScope_Global,
                              0 /* bus */,
                              (float)normalizedVolume,
                              0) == noErr)
        m_cachedVolume = normalizedVolume;
#else
    m_cachedVolume = normalizedVolume;
#endif
}

qreal CoreAudioOutput::volume() const
{
    return m_cachedVolume;
}

void CoreAudioOutput::setCategory(const QString &category)
{
    Q_UNUSED(category);
}

QString CoreAudioOutput::category() const
{
    return QString();
}

void CoreAudioOutput::deviceStopped()
{
    m_intervalTimer->stop();
    emit stateChanged(m_stateCode);
}

void CoreAudioOutput::inputReady()
{
    QMutexLocker lock(&m_mutex);
    if (m_stateCode == QAudio::IdleState) {
        audioThreadStart();

        m_stateCode = QAudio::ActiveState;
        m_errorCode = QAudio::NoError;

        emit stateChanged(m_stateCode);
    }
}

OSStatus CoreAudioOutput::renderCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
{
    Q_UNUSED(ioActionFlags)
    Q_UNUSED(inTimeStamp)
    Q_UNUSED(inBusNumber)
    Q_UNUSED(inNumberFrames)

    CoreAudioOutput* d = static_cast<CoreAudioOutput*>(inRefCon);

    const int threadState = d->m_audioThreadState.fetchAndAddAcquire(0);
    if (threadState == Stopped) {
        ioData->mBuffers[0].mDataByteSize = 0;
        d->audioDeviceStop();
    }
    else {
        const UInt32 bytesPerFrame = d->m_streamFormat.mBytesPerFrame;
        qint64 framesRead;

        framesRead = d->m_audioBuffer->readFrames((char*)ioData->mBuffers[0].mData,
                                                 ioData->mBuffers[0].mDataByteSize / bytesPerFrame);

        if (framesRead > 0) {
            ioData->mBuffers[0].mDataByteSize = framesRead * bytesPerFrame;
            d->m_totalFrames += framesRead;
#if defined(Q_OS_IOS) || defined(Q_OS_TVOS)
        // on iOS we have to adjust the sound volume ourselves
        if (!qFuzzyCompare(d->m_cachedVolume, qreal(1.0f))) {
            QAudioHelperInternal::qMultiplySamples(d->m_cachedVolume,
                                                   d->m_audioFormat,
                                                   ioData->mBuffers[0].mData, /* input */
                                                   ioData->mBuffers[0].mData, /* output */
                                                   ioData->mBuffers[0].mDataByteSize);
        }
#endif

        }
        else {
            ioData->mBuffers[0].mDataByteSize = 0;
            if (framesRead == 0) {
                if (threadState == Draining)
                    d->audioDeviceStop();
                else
                    d->audioDeviceIdle();
            }
            else
                d->audioDeviceError();
        }
    }

    return noErr;
}

bool CoreAudioOutput::open()
{
    if (m_errorCode != QAudio::NoError)
        return false;

    if (m_isOpen)
        return true;

    AudioComponentDescription componentDescription;
    componentDescription.componentType = kAudioUnitType_Output;
#if defined(Q_OS_OSX)
    componentDescription.componentSubType = kAudioUnitSubType_HALOutput;
#else
    componentDescription.componentSubType = kAudioUnitSubType_RemoteIO;
#endif
    componentDescription.componentManufacturer = kAudioUnitManufacturer_Apple;
    componentDescription.componentFlags = 0;
    componentDescription.componentFlagsMask = 0;

    AudioComponent component = AudioComponentFindNext(0, &componentDescription);
    if (component == 0) {
        qWarning() << "QAudioOutput: Failed to find Output component";
        return false;
    }

    if (AudioComponentInstanceNew(component, &m_audioUnit) != noErr) {
        qWarning() << "QAudioOutput: Unable to Open Output Component";
        return false;
    }

    // register callback
    AURenderCallbackStruct callback;
    callback.inputProc = renderCallback;
    callback.inputProcRefCon = this;

    if (AudioUnitSetProperty(m_audioUnit,
                             kAudioUnitProperty_SetRenderCallback,
                             kAudioUnitScope_Global,
                             0,
                             &callback,
                             sizeof(callback)) != noErr) {
        qWarning() << "QAudioOutput: Failed to set AudioUnit callback";
        return false;
    }

#if defined(Q_OS_OSX)
    //Set Audio Device
    if (AudioUnitSetProperty(m_audioUnit,
                             kAudioOutputUnitProperty_CurrentDevice,
                             kAudioUnitScope_Global,
                             0,
                             &m_audioDeviceId,
                             sizeof(m_audioDeviceId)) != noErr) {
        qWarning() << "QAudioOutput: Unable to use configured device";
        return false;
    }
#endif

    // Set stream format
    m_streamFormat = CoreAudioUtils::toAudioStreamBasicDescription(m_audioFormat);

    UInt32 size = sizeof(m_streamFormat);
    if (AudioUnitSetProperty(m_audioUnit,
                                kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Input,
                                0,
                                &m_streamFormat,
                                size) != noErr) {
        qWarning() << "QAudioOutput: Unable to Set Stream information";
        return false;
    }

    // Allocate buffer
    UInt32 numberOfFrames = 0;
#if defined(Q_OS_OSX)
    size = sizeof(UInt32);
    if (AudioUnitGetProperty(m_audioUnit,
                             kAudioDevicePropertyBufferFrameSize,
                             kAudioUnitScope_Global,
                             0,
                             &numberOfFrames,
                             &size) != noErr) {
        qWarning() << "QAudioInput: Failed to get audio period size";
        return false;
    }
#else //iOS
    Float32 bufferSize = CoreAudioSessionManager::instance().currentIOBufferDuration();
    bufferSize *= m_streamFormat.mSampleRate;
    numberOfFrames = bufferSize;
#endif

    m_periodSizeBytes = numberOfFrames * m_streamFormat.mBytesPerFrame;
    if (m_internalBufferSize < m_periodSizeBytes * 2)
        m_internalBufferSize = m_periodSizeBytes * 2;
    else
        m_internalBufferSize -= m_internalBufferSize % m_streamFormat.mBytesPerFrame;

    m_audioBuffer = new CoreAudioOutputBuffer(m_internalBufferSize, m_periodSizeBytes, m_audioFormat);
    connect(m_audioBuffer, SIGNAL(readyRead()), SLOT(inputReady())); //Pull

    m_audioIO = new CoreAudioOutputDevice(m_audioBuffer, this);

    //Init
    if (AudioUnitInitialize(m_audioUnit)) {
        qWarning() << "QAudioOutput: Failed to initialize AudioUnit";
        return false;
    }

    m_isOpen = true;

    setVolume(m_cachedVolume);

    return true;
}

void CoreAudioOutput::close()
{
    if (m_audioUnit != 0) {
        AudioOutputUnitStop(m_audioUnit);
        AudioUnitUninitialize(m_audioUnit);
        AudioComponentInstanceDispose(m_audioUnit);
    }

    delete m_audioBuffer;
}

void CoreAudioOutput::audioThreadStart()
{
    startTimers();
    m_audioThreadState.store(Running);
    AudioOutputUnitStart(m_audioUnit);
}

void CoreAudioOutput::audioThreadStop()
{
    stopTimers();
    if (m_audioThreadState.testAndSetAcquire(Running, Stopped))
        m_threadFinished.wait(&m_mutex, 500);
}

void CoreAudioOutput::audioThreadDrain()
{
    stopTimers();
    if (m_audioThreadState.testAndSetAcquire(Running, Draining))
        m_threadFinished.wait(&m_mutex, 500);
}

void CoreAudioOutput::audioDeviceStop()
{
    AudioOutputUnitStop(m_audioUnit);
    m_audioThreadState.store(Stopped);
    m_threadFinished.wakeOne();
}

void CoreAudioOutput::audioDeviceIdle()
{
    if (m_stateCode == QAudio::ActiveState) {
        QMutexLocker lock(&m_mutex);
        audioDeviceStop();

        m_errorCode = QAudio::UnderrunError;
        m_stateCode = QAudio::IdleState;
        QMetaObject::invokeMethod(this, "deviceStopped", Qt::QueuedConnection);
    }
}

void CoreAudioOutput::audioDeviceError()
{
    if (m_stateCode == QAudio::ActiveState) {
        QMutexLocker lock(&m_mutex);
        audioDeviceStop();

        m_errorCode = QAudio::IOError;
        m_stateCode = QAudio::StoppedState;
        QMetaObject::invokeMethod(this, "deviceStopped", Qt::QueuedConnection);
    }
}

void CoreAudioOutput::startTimers()
{
    m_audioBuffer->startFillTimer();
    if (m_intervalTimer->interval() > 0)
        m_intervalTimer->start();
}

void CoreAudioOutput::stopTimers()
{
    m_audioBuffer->stopFillTimer();
    m_intervalTimer->stop();
}

QT_END_NAMESPACE

#include "moc_coreaudiooutput.cpp"
