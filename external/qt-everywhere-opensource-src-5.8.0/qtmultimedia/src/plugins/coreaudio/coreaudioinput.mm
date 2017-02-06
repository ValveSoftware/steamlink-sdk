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
#include "coreaudioinput.h"
#include "coreaudiosessionmanager.h"
#include "coreaudiodeviceinfo.h"
#include "coreaudioutils.h"

#if defined(Q_OS_OSX)
# include <AudioUnit/AudioComponent.h>
#endif

#if defined(Q_OS_IOS) || defined(Q_OS_TVOS)
# include "coreaudiosessionmanager.h"
#endif

#include <QtMultimedia/private/qaudiohelpers_p.h>
#include <QtCore/QDataStream>
#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

static const int DEFAULT_BUFFER_SIZE = 4 * 1024;

CoreAudioBufferList::CoreAudioBufferList(const AudioStreamBasicDescription &streamFormat)
    : m_owner(false)
    , m_streamDescription(streamFormat)
{
    const bool isInterleaved = (m_streamDescription.mFormatFlags & kAudioFormatFlagIsNonInterleaved) == 0;
    const int numberOfBuffers = isInterleaved ? 1 : m_streamDescription.mChannelsPerFrame;

    m_dataSize = 0;

    m_bufferList = reinterpret_cast<AudioBufferList*>(malloc(sizeof(AudioBufferList) +
                                                            (sizeof(AudioBuffer) * numberOfBuffers)));

    m_bufferList->mNumberBuffers = numberOfBuffers;
    for (int i = 0; i < numberOfBuffers; ++i) {
        m_bufferList->mBuffers[i].mNumberChannels = isInterleaved ? numberOfBuffers : 1;
        m_bufferList->mBuffers[i].mDataByteSize = 0;
        m_bufferList->mBuffers[i].mData = 0;
    }
}

CoreAudioBufferList::CoreAudioBufferList(const AudioStreamBasicDescription &streamFormat, char *buffer, int bufferSize)
    : m_owner(false)
    , m_streamDescription(streamFormat)
    , m_bufferList(0)
{
    m_dataSize = bufferSize;

    m_bufferList = reinterpret_cast<AudioBufferList*>(malloc(sizeof(AudioBufferList) + sizeof(AudioBuffer)));

    m_bufferList->mNumberBuffers = 1;
    m_bufferList->mBuffers[0].mNumberChannels = 1;
    m_bufferList->mBuffers[0].mDataByteSize = m_dataSize;
    m_bufferList->mBuffers[0].mData = buffer;
}

CoreAudioBufferList::CoreAudioBufferList(const AudioStreamBasicDescription &streamFormat, int framesToBuffer)
    : m_owner(true)
    , m_streamDescription(streamFormat)
    , m_bufferList(0)
{
    const bool isInterleaved = (m_streamDescription.mFormatFlags & kAudioFormatFlagIsNonInterleaved) == 0;
    const int numberOfBuffers = isInterleaved ? 1 : m_streamDescription.mChannelsPerFrame;

    m_dataSize = framesToBuffer * m_streamDescription.mBytesPerFrame;

    m_bufferList = reinterpret_cast<AudioBufferList*>(malloc(sizeof(AudioBufferList) +
                                                            (sizeof(AudioBuffer) * numberOfBuffers)));
    m_bufferList->mNumberBuffers = numberOfBuffers;
    for (int i = 0; i < numberOfBuffers; ++i) {
        m_bufferList->mBuffers[i].mNumberChannels = isInterleaved ? numberOfBuffers : 1;
        m_bufferList->mBuffers[i].mDataByteSize = m_dataSize;
        m_bufferList->mBuffers[i].mData = malloc(m_dataSize);
    }
}

CoreAudioBufferList::~CoreAudioBufferList()
{
    if (m_owner) {
        for (UInt32 i = 0; i < m_bufferList->mNumberBuffers; ++i)
            free(m_bufferList->mBuffers[i].mData);
    }

    free(m_bufferList);
}

char *CoreAudioBufferList::data(int buffer) const
{
    return static_cast<char*>(m_bufferList->mBuffers[buffer].mData);
}

qint64 CoreAudioBufferList::bufferSize(int buffer) const
{
    return m_bufferList->mBuffers[buffer].mDataByteSize;
}

int CoreAudioBufferList::frameCount(int buffer) const
{
    return m_bufferList->mBuffers[buffer].mDataByteSize / m_streamDescription.mBytesPerFrame;
}

int CoreAudioBufferList::packetCount(int buffer) const
{
    return m_bufferList->mBuffers[buffer].mDataByteSize / m_streamDescription.mBytesPerPacket;
}

int CoreAudioBufferList::packetSize() const
{
    return m_streamDescription.mBytesPerPacket;
}

void CoreAudioBufferList::reset()
{
    for (UInt32 i = 0; i < m_bufferList->mNumberBuffers; ++i) {
        m_bufferList->mBuffers[i].mDataByteSize = m_dataSize;
        m_bufferList->mBuffers[i].mData = 0;
    }
}

CoreAudioPacketFeeder::CoreAudioPacketFeeder(CoreAudioBufferList *abl)
    : m_audioBufferList(abl)
{
    m_totalPackets = m_audioBufferList->packetCount();
    m_position = 0;
}

bool CoreAudioPacketFeeder::feed(AudioBufferList &dst, UInt32 &packetCount)
{
    if (m_position == m_totalPackets) {
        dst.mBuffers[0].mDataByteSize = 0;
        packetCount = 0;
        return false;
    }

    if (m_totalPackets - m_position < packetCount)
        packetCount = m_totalPackets - m_position;

    dst.mBuffers[0].mDataByteSize = packetCount * m_audioBufferList->packetSize();
    dst.mBuffers[0].mData = m_audioBufferList->data() + (m_position * m_audioBufferList->packetSize());

    m_position += packetCount;

    return true;
}

bool CoreAudioPacketFeeder::empty() const
{
    return m_position == m_totalPackets;
}

CoreAudioInputBuffer::CoreAudioInputBuffer(int bufferSize, int maxPeriodSize, const AudioStreamBasicDescription &inputFormat, const AudioStreamBasicDescription &outputFormat, QObject *parent)
    : QObject(parent)
    , m_deviceError(false)
    , m_device(0)
    , m_audioConverter(0)
    , m_inputFormat(inputFormat)
    , m_outputFormat(outputFormat)
    , m_volume(qreal(1.0f))
{
    m_maxPeriodSize = maxPeriodSize;
    m_periodTime = m_maxPeriodSize / m_outputFormat.mBytesPerFrame * 1000 / m_outputFormat.mSampleRate;

    m_buffer = new CoreAudioRingBuffer(bufferSize);

    m_inputBufferList = new CoreAudioBufferList(m_inputFormat);

    m_flushTimer = new QTimer(this);
    connect(m_flushTimer, SIGNAL(timeout()), SLOT(flushBuffer()));

    if (CoreAudioUtils::toQAudioFormat(inputFormat) != CoreAudioUtils::toQAudioFormat(outputFormat)) {
        if (AudioConverterNew(&m_inputFormat, &m_outputFormat, &m_audioConverter) != noErr) {
            qWarning() << "QAudioInput: Unable to create an Audio Converter";
            m_audioConverter = 0;
        }
    }

    m_qFormat = CoreAudioUtils::toQAudioFormat(inputFormat); // we adjust volume before conversion
}

CoreAudioInputBuffer::~CoreAudioInputBuffer()
{
    delete m_buffer;
}

qreal CoreAudioInputBuffer::volume() const
{
    return m_volume;
}

void CoreAudioInputBuffer::setVolume(qreal v)
{
    m_volume = v;
}

qint64 CoreAudioInputBuffer::renderFromDevice(AudioUnit audioUnit, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames)
{
    const bool  pullMode = m_device == 0;

    OSStatus    err;
    qint64      framesRendered = 0;

    m_inputBufferList->reset();
    err = AudioUnitRender(audioUnit,
                          ioActionFlags,
                          inTimeStamp,
                          inBusNumber,
                          inNumberFrames,
                          m_inputBufferList->audioBufferList());

    // adjust volume, if necessary
    if (!qFuzzyCompare(m_volume, qreal(1.0f))) {
        QAudioHelperInternal::qMultiplySamples(m_volume,
                                               m_qFormat,
                                               m_inputBufferList->data(), /* input */
                                               m_inputBufferList->data(), /* output */
                                               m_inputBufferList->bufferSize());
    }

    if (m_audioConverter != 0) {
        CoreAudioPacketFeeder  feeder(m_inputBufferList);

        int     copied = 0;
        const int available = m_buffer->free();

        while (err == noErr && !feeder.empty()) {
            CoreAudioRingBuffer::Region region = m_buffer->acquireWriteRegion(available - copied);

            if (region.second == 0)
                break;

            AudioBufferList     output;
            output.mNumberBuffers = 1;
            output.mBuffers[0].mNumberChannels = 1;
            output.mBuffers[0].mDataByteSize = region.second;
            output.mBuffers[0].mData = region.first;

            UInt32  packetSize = region.second / m_outputFormat.mBytesPerPacket;
            err = AudioConverterFillComplexBuffer(m_audioConverter,
                                                  converterCallback,
                                                  &feeder,
                                                  &packetSize,
                                                  &output,
                                                  0);
            region.second = output.mBuffers[0].mDataByteSize;
            copied += region.second;

            m_buffer->releaseWriteRegion(region);
        }

        framesRendered += copied / m_outputFormat.mBytesPerFrame;
    }
    else {
        const int available = m_inputBufferList->bufferSize();
        bool    wecan = true;
        int     copied = 0;

        while (wecan && copied < available) {
            CoreAudioRingBuffer::Region region = m_buffer->acquireWriteRegion(available - copied);

            if (region.second > 0) {
                memcpy(region.first, m_inputBufferList->data() + copied, region.second);
                copied += region.second;
            }
            else
                wecan = false;

            m_buffer->releaseWriteRegion(region);
        }

        framesRendered = copied / m_outputFormat.mBytesPerFrame;
    }

    if (pullMode && framesRendered > 0)
        emit readyRead();

    return framesRendered;
}

qint64 CoreAudioInputBuffer::readBytes(char *data, qint64 len)
{
    bool    wecan = true;
    qint64  bytesCopied = 0;

    len -= len % m_maxPeriodSize;
    while (wecan && bytesCopied < len) {
        CoreAudioRingBuffer::Region region = m_buffer->acquireReadRegion(len - bytesCopied);

        if (region.second > 0) {
            memcpy(data + bytesCopied, region.first, region.second);
            bytesCopied += region.second;
        }
        else
            wecan = false;

        m_buffer->releaseReadRegion(region);
    }

    return bytesCopied;
}

void CoreAudioInputBuffer::setFlushDevice(QIODevice *device)
{
    if (m_device != device)
        m_device = device;
}

void CoreAudioInputBuffer::startFlushTimer()
{
    if (m_device != 0) {
        // We use the period time for the timer, since that's
        // around the buffer size (pre conversion >.>)
        m_flushTimer->start(qMax(1, m_periodTime));
    }
}

void CoreAudioInputBuffer::stopFlushTimer()
{
    m_flushTimer->stop();
}

void CoreAudioInputBuffer::flush(bool all)
{
    if (m_device == 0)
        return;

    const int used = m_buffer->used();
    const int readSize = all ? used : used - (used % m_maxPeriodSize);

    if (readSize > 0) {
        bool    wecan = true;
        int     flushed = 0;

        while (!m_deviceError && wecan && flushed < readSize) {
            CoreAudioRingBuffer::Region region = m_buffer->acquireReadRegion(readSize - flushed);

            if (region.second > 0) {
                int bytesWritten = m_device->write(region.first, region.second);
                if (bytesWritten < 0) {
                    stopFlushTimer();
                    m_deviceError = true;
                }
                else {
                    region.second = bytesWritten;
                    flushed += bytesWritten;
                    wecan = bytesWritten != 0;
                }
            }
            else
                wecan = false;

            m_buffer->releaseReadRegion(region);
        }
    }
}

void CoreAudioInputBuffer::reset()
{
    m_buffer->reset();
    m_deviceError = false;
}

int CoreAudioInputBuffer::available() const
{
    return m_buffer->free();
}

int CoreAudioInputBuffer::used() const
{
    return m_buffer->used();
}

void CoreAudioInputBuffer::flushBuffer()
{
    flush();
}

OSStatus CoreAudioInputBuffer::converterCallback(AudioConverterRef inAudioConverter, UInt32 *ioNumberDataPackets, AudioBufferList *ioData, AudioStreamPacketDescription **outDataPacketDescription, void *inUserData)
{
    Q_UNUSED(inAudioConverter);
    Q_UNUSED(outDataPacketDescription);

    CoreAudioPacketFeeder* feeder = static_cast<CoreAudioPacketFeeder*>(inUserData);

    if (!feeder->feed(*ioData, *ioNumberDataPackets))
        return as_empty;

    return noErr;
}

CoreAudioInputDevice::CoreAudioInputDevice(CoreAudioInputBuffer *audioBuffer, QObject *parent)
    : QIODevice(parent)
    , m_audioBuffer(audioBuffer)
{
    open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    connect(m_audioBuffer, SIGNAL(readyRead()), SIGNAL(readyRead()));
}

qint64 CoreAudioInputDevice::readData(char *data, qint64 len)
{
    return m_audioBuffer->readBytes(data, len);
}

qint64 CoreAudioInputDevice::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);

    return 0;
}

CoreAudioInput::CoreAudioInput(const QByteArray &device)
    : m_isOpen(false)
    , m_internalBufferSize(DEFAULT_BUFFER_SIZE)
    , m_totalFrames(0)
    , m_audioUnit(0)
    , m_clockFrequency(CoreAudioUtils::frequency() / 1000)
    , m_startTime(0)
    , m_errorCode(QAudio::NoError)
    , m_stateCode(QAudio::StoppedState)
    , m_audioBuffer(0)
    , m_volume(1.0)
{
#if defined(Q_OS_OSX)
    quint32 deviceId;
    QDataStream dataStream(device);
    dataStream >> deviceId >> m_device;
    m_audioDeviceId = AudioDeviceID(deviceId);
#else //iOS
    m_device = device;
#endif

    m_audioDeviceInfo = new CoreAudioDeviceInfo(device, QAudio::AudioInput);

    m_intervalTimer = new QTimer(this);
    m_intervalTimer->setInterval(1000);
    connect(m_intervalTimer, SIGNAL(timeout()), this, SIGNAL(notify()));
}


CoreAudioInput::~CoreAudioInput()
{
    close();
    delete m_audioDeviceInfo;
}

bool CoreAudioInput::open()
{
    if (m_isOpen)
        return true;

    UInt32  size = 0;

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
        qWarning() << "QAudioInput: Failed to find Output component";
        return false;
    }

    if (AudioComponentInstanceNew(component, &m_audioUnit) != noErr) {
        qWarning() << "QAudioInput: Unable to Open Output Component";
        return false;
    }

    // Set mode
    // switch to input mode
    UInt32 enable = 1;
    if (AudioUnitSetProperty(m_audioUnit,
                               kAudioOutputUnitProperty_EnableIO,
                               kAudioUnitScope_Input,
                               1,
                               &enable,
                               sizeof(enable)) != noErr) {
        qWarning() << "QAudioInput: Unable to switch to input mode (Enable Input)";
        return false;
    }

    enable = 0;
    if (AudioUnitSetProperty(m_audioUnit,
                            kAudioOutputUnitProperty_EnableIO,
                            kAudioUnitScope_Output,
                            0,
                            &enable,
                            sizeof(enable)) != noErr) {
        qWarning() << "QAudioInput: Unable to switch to input mode (Disable output)";
        return false;
    }

    // register callback
    AURenderCallbackStruct callback;
    callback.inputProc = inputCallback;
    callback.inputProcRefCon = this;

    if (AudioUnitSetProperty(m_audioUnit,
                               kAudioOutputUnitProperty_SetInputCallback,
                               kAudioUnitScope_Global,
                               0,
                               &callback,
                               sizeof(callback)) != noErr) {
        qWarning() << "QAudioInput: Failed to set AudioUnit callback";
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
        qWarning() << "QAudioInput: Unable to use configured device";
        return false;
    }
#endif

    //set format
    m_streamFormat = CoreAudioUtils::toAudioStreamBasicDescription(m_audioFormat);

#if defined(Q_OS_OSX)
    if (m_audioFormat == m_audioDeviceInfo->preferredFormat()) {
#endif

    m_deviceFormat = m_streamFormat;
    AudioUnitSetProperty(m_audioUnit,
                         kAudioUnitProperty_StreamFormat,
                         kAudioUnitScope_Output,
                         1,
                         &m_deviceFormat,
                         sizeof(m_deviceFormat));
#if defined(Q_OS_OSX)
    } else {
        size = sizeof(m_deviceFormat);
        if (AudioUnitGetProperty(m_audioUnit,
                                 kAudioUnitProperty_StreamFormat,
                                 kAudioUnitScope_Input,
                                 1,
                                 &m_deviceFormat,
                                 &size) != noErr) {
            qWarning() << "QAudioInput: Unable to retrieve device format";
            return false;
        }

        if (AudioUnitSetProperty(m_audioUnit,
                                 kAudioUnitProperty_StreamFormat,
                                 kAudioUnitScope_Output,
                                 1,
                                 &m_deviceFormat,
                                 sizeof(m_deviceFormat)) != noErr) {
            qWarning() << "QAudioInput: Unable to set device format";
            return false;
        }
    }
#endif

    //setup buffers
    UInt32 numberOfFrames;
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
    //BUG: numberOfFrames gets ignored after this point

    AudioValueRange bufferRange;
    size = sizeof(AudioValueRange);

    if (AudioUnitGetProperty(m_audioUnit,
                             kAudioDevicePropertyBufferFrameSizeRange,
                             kAudioUnitScope_Global,
                             0,
                             &bufferRange,
                             &size) != noErr) {
        qWarning() << "QAudioInput: Failed to get audio period size range";
        return false;
    }

    // See if the requested buffer size is permissible
    numberOfFrames = qBound((UInt32)bufferRange.mMinimum, m_internalBufferSize / m_streamFormat.mBytesPerFrame, (UInt32)bufferRange.mMaximum);

    // Set it back
    if (AudioUnitSetProperty(m_audioUnit,
                             kAudioDevicePropertyBufferFrameSize,
                             kAudioUnitScope_Global,
                             0,
                             &numberOfFrames,
                             sizeof(UInt32)) != noErr) {
        qWarning() << "QAudioInput: Failed to set audio buffer size";
        return false;
    }
#else //iOS
    Float32 bufferSize = CoreAudioSessionManager::instance().currentIOBufferDuration();
    bufferSize *= m_streamFormat.mSampleRate;
    numberOfFrames = bufferSize;
#endif

    // Now allocate a few buffers to be safe.
    m_periodSizeBytes = m_internalBufferSize = numberOfFrames * m_streamFormat.mBytesPerFrame;

    m_audioBuffer = new CoreAudioInputBuffer(m_internalBufferSize * 4,
                                        m_periodSizeBytes,
                                        m_deviceFormat,
                                        m_streamFormat,
                                        this);

    m_audioBuffer->setVolume(m_volume);
    m_audioIO = new CoreAudioInputDevice(m_audioBuffer, this);

    // Init
    if (AudioUnitInitialize(m_audioUnit) != noErr) {
        qWarning() << "QAudioInput: Failed to initialize AudioUnit";
        return false;
    }

    m_isOpen = true;

    return m_isOpen;

}

void CoreAudioInput::close()
{
    if (m_audioUnit != 0) {
        AudioOutputUnitStop(m_audioUnit);
        AudioUnitUninitialize(m_audioUnit);
        AudioComponentInstanceDispose(m_audioUnit);
    }

    delete m_audioBuffer;
}

void CoreAudioInput::start(QIODevice *device)
{
    QIODevice* op = device;

    if (!m_audioDeviceInfo->isFormatSupported(m_audioFormat) || !open()) {
        m_stateCode = QAudio::StoppedState;
        m_errorCode = QAudio::OpenError;
        return;
    }

    reset();
    m_audioBuffer->reset();
    m_audioBuffer->setFlushDevice(op);

    if (op == 0)
        op = m_audioIO;

    // Start
    m_startTime = CoreAudioUtils::currentTime();
    m_totalFrames = 0;

    m_stateCode = QAudio::IdleState;
    m_errorCode = QAudio::NoError;
    emit stateChanged(m_stateCode);

    audioThreadStart();
}


QIODevice *CoreAudioInput::start()
{
    QIODevice* op = 0;

    if (!m_audioDeviceInfo->isFormatSupported(m_audioFormat) || !open()) {
        m_stateCode = QAudio::StoppedState;
        m_errorCode = QAudio::OpenError;
        return m_audioIO;
    }

    reset();
    m_audioBuffer->reset();
    m_audioBuffer->setFlushDevice(op);

    if (op == 0)
        op = m_audioIO;

    // Start
    m_startTime = CoreAudioUtils::currentTime();
    m_totalFrames = 0;

    m_stateCode = QAudio::IdleState;
    m_errorCode = QAudio::NoError;
    emit stateChanged(m_stateCode);

    audioThreadStart();

    return op;
}


void CoreAudioInput::stop()
{
    QMutexLocker lock(&m_mutex);
    if (m_stateCode != QAudio::StoppedState) {
        audioThreadStop();
        m_audioBuffer->flush(true);

        m_errorCode = QAudio::NoError;
        m_stateCode = QAudio::StoppedState;
        QMetaObject::invokeMethod(this, "stateChanged", Qt::QueuedConnection, Q_ARG(QAudio::State, m_stateCode));
    }
}


void CoreAudioInput::reset()
{
    QMutexLocker lock(&m_mutex);
    if (m_stateCode != QAudio::StoppedState) {
        audioThreadStop();

        m_errorCode = QAudio::NoError;
        m_stateCode = QAudio::StoppedState;
        m_audioBuffer->reset();
        QMetaObject::invokeMethod(this, "stateChanged", Qt::QueuedConnection, Q_ARG(QAudio::State, m_stateCode));
    }
}


void CoreAudioInput::suspend()
{
    QMutexLocker lock(&m_mutex);
    if (m_stateCode == QAudio::ActiveState || m_stateCode == QAudio::IdleState) {
        audioThreadStop();

        m_errorCode = QAudio::NoError;
        m_stateCode = QAudio::SuspendedState;
        QMetaObject::invokeMethod(this, "stateChanged", Qt::QueuedConnection, Q_ARG(QAudio::State, m_stateCode));
    }
}


void CoreAudioInput::resume()
{
    QMutexLocker lock(&m_mutex);
    if (m_stateCode == QAudio::SuspendedState) {
        audioThreadStart();

        m_errorCode = QAudio::NoError;
        m_stateCode = QAudio::ActiveState;
        QMetaObject::invokeMethod(this, "stateChanged", Qt::QueuedConnection, Q_ARG(QAudio::State, m_stateCode));
    }
}


int CoreAudioInput::bytesReady() const
{
    if (!m_audioBuffer)
        return 0;
    return m_audioBuffer->used();
}


int CoreAudioInput::periodSize() const
{
    return m_periodSizeBytes;
}


void CoreAudioInput::setBufferSize(int value)
{
    m_internalBufferSize = value;
}


int CoreAudioInput::bufferSize() const
{
    return m_internalBufferSize;
}


void CoreAudioInput::setNotifyInterval(int milliSeconds)
{
    if (m_intervalTimer->interval() == milliSeconds)
        return;

    if (milliSeconds <= 0)
        milliSeconds = 0;

    m_intervalTimer->setInterval(milliSeconds);
}


int CoreAudioInput::notifyInterval() const
{
    return m_intervalTimer->interval();
}


qint64 CoreAudioInput::processedUSecs() const
{
    return m_totalFrames * 1000000 / m_audioFormat.sampleRate();
}


qint64 CoreAudioInput::elapsedUSecs() const
{
    if (m_stateCode == QAudio::StoppedState)
        return 0;

    return (CoreAudioUtils::currentTime() - m_startTime) / (m_clockFrequency / 1000);
}


QAudio::Error CoreAudioInput::error() const
{
    return m_errorCode;
}


QAudio::State CoreAudioInput::state() const
{
    return m_stateCode;
}


void CoreAudioInput::setFormat(const QAudioFormat &format)
{
    if (m_stateCode == QAudio::StoppedState)
        m_audioFormat = format;
}


QAudioFormat CoreAudioInput::format() const
{
    return m_audioFormat;
}


void CoreAudioInput::setVolume(qreal volume)
{
    m_volume = volume;
    if (m_audioBuffer)
        m_audioBuffer->setVolume(m_volume);
}


qreal CoreAudioInput::volume() const
{
    return m_volume;
}

void CoreAudioInput::deviceStoppped()
{
    stopTimers();
    emit stateChanged(m_stateCode);
}

void CoreAudioInput::audioThreadStart()
{
    startTimers();
    m_audioThreadState.store(Running);
    AudioOutputUnitStart(m_audioUnit);
}

void CoreAudioInput::audioThreadStop()
{
    stopTimers();
    if (m_audioThreadState.testAndSetAcquire(Running, Stopped))
        m_threadFinished.wait(&m_mutex);
}

void CoreAudioInput::audioDeviceStop()
{
    AudioOutputUnitStop(m_audioUnit);
    m_audioThreadState.store(Stopped);
    m_threadFinished.wakeOne();
}

void CoreAudioInput::audioDeviceActive()
{
    if (m_stateCode == QAudio::IdleState) {
        QMutexLocker lock(&m_mutex);
        m_stateCode = QAudio::ActiveState;
        emit stateChanged(m_stateCode);
    }
}

void CoreAudioInput::audioDeviceFull()
{
    if (m_stateCode == QAudio::ActiveState) {
        QMutexLocker lock(&m_mutex);
        m_errorCode = QAudio::UnderrunError;
        m_stateCode = QAudio::IdleState;
        emit stateChanged(m_stateCode);
    }
}

void CoreAudioInput::audioDeviceError()
{
    if (m_stateCode == QAudio::ActiveState) {
        QMutexLocker lock(&m_mutex);
        audioDeviceStop();

        m_errorCode = QAudio::IOError;
        m_stateCode = QAudio::StoppedState;
        QMetaObject::invokeMethod(this, "deviceStopped", Qt::QueuedConnection);
    }
}

void CoreAudioInput::startTimers()
{
    m_audioBuffer->startFlushTimer();
    if (m_intervalTimer->interval() > 0)
        m_intervalTimer->start();
}

void CoreAudioInput::stopTimers()
{
    m_audioBuffer->stopFlushTimer();
    m_intervalTimer->stop();
}

OSStatus CoreAudioInput::inputCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
{
    Q_UNUSED(ioData);

    CoreAudioInput* d = static_cast<CoreAudioInput*>(inRefCon);

    const int threadState = d->m_audioThreadState.loadAcquire();
    if (threadState == Stopped)
        d->audioDeviceStop();
    else {
        qint64 framesWritten;

        framesWritten = d->m_audioBuffer->renderFromDevice(d->m_audioUnit,
                                                         ioActionFlags,
                                                         inTimeStamp,
                                                         inBusNumber,
                                                         inNumberFrames);

        if (framesWritten > 0) {
            d->m_totalFrames += framesWritten;
            d->audioDeviceActive();
        } else if (framesWritten == 0)
            d->audioDeviceFull();
        else if (framesWritten < 0)
            d->audioDeviceError();
    }

    return noErr;
}

QT_END_NAMESPACE

#include "moc_coreaudioinput.cpp"
