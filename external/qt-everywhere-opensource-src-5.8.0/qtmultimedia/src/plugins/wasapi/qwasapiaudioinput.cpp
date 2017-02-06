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

#include "qwasapiaudioinput.h"
#include "qwasapiutils.h"

#include <QtCore/QMutexLocker>
#include <QtCore/QThread>
#include <QtCore/qfunctions_winrt.h>

#include <Audioclient.h>

using namespace Microsoft::WRL;

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcMmAudioInput, "qt.multimedia.audioinput")

class WasapiInputDevicePrivate : public QIODevice
{
    Q_OBJECT
public:
    WasapiInputDevicePrivate(QWasapiAudioInput* input);
    ~WasapiInputDevicePrivate();

    qint64 readData(char* data, qint64 len);
    qint64 writeData(const char* data, qint64 len);

private:
    QWasapiAudioInput *m_input;
};

WasapiInputDevicePrivate::WasapiInputDevicePrivate(QWasapiAudioInput* input)
    : m_input(input)
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
}

WasapiInputDevicePrivate::~WasapiInputDevicePrivate()
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
}

qint64 WasapiInputDevicePrivate::readData(char* data, qint64 len)
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;

    const quint32 channelCount = m_input->m_currentFormat.channelCount();
    const quint32 sampleBytes = m_input->m_currentFormat.sampleSize() / 8;

    const quint32 requestedFrames = len / channelCount / sampleBytes;
    quint32 availableFrames = 0;
    HRESULT hr = m_input->m_capture->GetNextPacketSize(&availableFrames);

    quint32 readFrames = qMin(requestedFrames, availableFrames);
    const qint64 readBytes = readFrames * channelCount * sampleBytes;

    BYTE* buffer;
    DWORD flags;

    QMutexLocker locker(&m_input->m_mutex);

    quint64 devicePosition;
    hr = m_input->m_capture->GetBuffer(&buffer, &readFrames, &flags, &devicePosition, NULL);
    if (hr != S_OK) {
        m_input->m_currentError = QAudio::FatalError;
        emit m_input->errorChanged(m_input->m_currentError);
        hr = m_input->m_capture->ReleaseBuffer(readFrames);
        qCDebug(lcMmAudioInput) << __FUNCTION__ << "Could not acquire input buffer.";
        return 0;
    }
    if (Q_UNLIKELY(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
        // In case this flag is set, user is supposed to ignore the content
        // of the buffer and manually write silence
        qCDebug(lcMmAudioInput) << __FUNCTION__ << "AUDCLNT_BUFFERFLAGS_SILENT: "
                                << "Ignoring buffer and writing silence.";
        memset(data, 0, readBytes);
    } else {
        memcpy(data, buffer, readBytes);
    }

    hr = m_input->m_capture->ReleaseBuffer(readFrames);
    if (hr != S_OK)
        qFatal("Could not release buffer");

    if (m_input->m_interval && m_input->m_openTime.elapsed() - m_input->m_openTimeOffset > m_input->m_interval) {
        emit m_input->notify();
        m_input->m_openTimeOffset = m_input->m_openTime.elapsed();
    }

    m_input->m_bytesProcessed += readBytes;

    if (m_input->m_currentState != QAudio::ActiveState) {
        m_input->m_currentState = QAudio::ActiveState;
        emit m_input->stateChanged(m_input->m_currentState);
    }
    return readBytes;
}

qint64 WasapiInputDevicePrivate::writeData(const char* data, qint64 len)
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
    Q_UNUSED(data)
    Q_UNUSED(len)
    return 0;
}

QWasapiAudioInput::QWasapiAudioInput(const QByteArray &device)
    : m_deviceName(device)
#if defined(CLASSIC_APP_BUILD) || _MSC_VER >= 1900
    , m_volumeCache(qreal(1.))
#endif
    , m_currentState(QAudio::StoppedState)
    , m_currentError(QAudio::NoError)
    , m_bufferFrames(0)
    , m_bufferBytes(4096)
    , m_eventThread(0)
{
    qCDebug(lcMmAudioInput) << __FUNCTION__ << device;
    Q_UNUSED(device)
}

QWasapiAudioInput::~QWasapiAudioInput()
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
    stop();
}

void QWasapiAudioInput::setVolume(qreal vol)
{
    qCDebug(lcMmAudioInput) << __FUNCTION__ << vol;
#if defined(CLASSIC_APP_BUILD) || _MSC_VER >= 1900 // Volume is only supported MSVC2015 and beyond
    m_volumeCache = vol;
    if (m_volumeControl) {
        quint32 channelCount;
        HRESULT hr = m_volumeControl->GetChannelCount(&channelCount);
        for (quint32 i = 0; i < channelCount; ++i) {
            hr = m_volumeControl->SetChannelVolume(i, vol);
            RETURN_VOID_IF_FAILED("Could not set audio volume.");
        }
    }
#endif // defined(CLASSIC_APP_BUILD) || _MSC_VER >= 1900
}

qreal QWasapiAudioInput::volume() const
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
#if defined(CLASSIC_APP_BUILD) || _MSC_VER >= 1900 // Volume is only supported MSVC2015 and beyond
    return m_volumeCache;
#else
    return qreal(1.0);
#endif
}

void QWasapiAudioInput::process()
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
    DWORD waitRet;

    m_processing = true;
    do {
        waitRet = WaitForSingleObjectEx(m_event, 2000, FALSE);
        if (waitRet != WAIT_OBJECT_0) {
            qFatal("Returned while waiting for event.");
            return;
        }

        QMutexLocker locker(&m_mutex);

        if (m_currentState != QAudio::ActiveState && m_currentState != QAudio::IdleState)
            break;
        QMetaObject::invokeMethod(this, "processBuffer", Qt::QueuedConnection);
    } while (m_processing);
}

void QWasapiAudioInput::processBuffer()
{
    if (!m_pullMode) {
        emit m_eventDevice->readyRead();
        ResetEvent(m_event);
        return;
    }

    QMutexLocker locker(&m_mutex);
    const quint32 channelCount = m_currentFormat.channelCount();
    const quint32 sampleBytes = m_currentFormat.sampleSize() / 8;
    BYTE* buffer;
    HRESULT hr;

    quint32 packetFrames;
    hr = m_capture->GetNextPacketSize(&packetFrames);

    while (packetFrames != 0 && m_currentState == QAudio::ActiveState) {
        DWORD flags;
        quint64 devicePosition;
        hr = m_capture->GetBuffer(&buffer, &packetFrames, &flags, &devicePosition, NULL);
        if (hr != S_OK) {
            m_currentError = QAudio::FatalError;
            emit errorChanged(m_currentError);
            // Also Error Buffers need to be released
            hr = m_capture->ReleaseBuffer(packetFrames);
            qCDebug(lcMmAudioInput) << __FUNCTION__ << "Could not acquire input buffer.";
            return;
        }
        const quint32 writeBytes = packetFrames * channelCount * sampleBytes;
        if (Q_UNLIKELY(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
            // In case this flag is set, user is supposed to ignore the content
            // of the buffer and manually write silence
            qCDebug(lcMmAudioInput) << __FUNCTION__ << "AUDCLNT_BUFFERFLAGS_SILENT: "
                                    << "Ignoring buffer and writing silence.";
            buffer = new BYTE[writeBytes];
            memset(buffer, 0, writeBytes);
        }

        const qint64 written = m_eventDevice->write(reinterpret_cast<const char *>(buffer), writeBytes);

        if (Q_UNLIKELY(flags & AUDCLNT_BUFFERFLAGS_SILENT))
            delete [] buffer;

        if (written < static_cast<qint64>(writeBytes)) {
            if (m_currentError != QAudio::UnderrunError) {
                m_currentError = QAudio::UnderrunError;
                emit errorChanged(m_currentError);
            }
        }
        hr = m_capture->ReleaseBuffer(packetFrames);
        if (hr != S_OK)
            qFatal("Could not release buffer");

        m_bytesProcessed += writeBytes;

        hr = m_capture->GetNextPacketSize(&packetFrames);
    }
    ResetEvent(m_event);

    if (m_interval && m_openTime.elapsed() - m_openTimeOffset > m_interval) {
        emit notify();
        m_openTimeOffset = m_openTime.elapsed();
    }
    m_processing = m_currentState == QAudio::ActiveState || m_currentState == QAudio::IdleState;
}

bool QWasapiAudioInput::initStart(bool pull)
{
    if (m_currentState == QAudio::ActiveState || m_currentState == QAudio::IdleState)
        stop();

    QMutexLocker locker(&m_mutex);

    m_interface = QWasapiUtils::createOrGetInterface(m_deviceName, QAudio::AudioInput);
    Q_ASSERT(m_interface);

    m_pullMode = pull;
    WAVEFORMATEX nFmt;
    WAVEFORMATEX closest;
    WAVEFORMATEX *pClose = &closest;

    if (!m_currentFormat.isValid() || !QWasapiUtils::convertToNativeFormat(m_currentFormat, &nFmt)) {
        m_currentError = QAudio::OpenError;
        emit errorChanged(m_currentError);
        return false;
    }

    HRESULT hr;

    hr = m_interface->m_client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &nFmt, &pClose);
    if (hr != S_OK) {
        m_currentError = QAudio::OpenError;
        emit errorChanged(m_currentError);
        return false;
    }

    REFERENCE_TIME t = ((10000.0 * 10000 / nFmt.nSamplesPerSec * 1024) + 0.5);
    if (m_bufferBytes)
        t = m_currentFormat.durationForBytes(m_bufferBytes) * 10;

    const DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    hr = m_interface->m_client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, t, 0, &nFmt, NULL);
    EMIT_RETURN_FALSE_IF_FAILED("Could not initialize audio client.", QAudio::OpenError)

    hr = m_interface->m_client->GetService(IID_PPV_ARGS(&m_capture));
    EMIT_RETURN_FALSE_IF_FAILED("Could not acquire render service.", QAudio::OpenError)

#if defined(CLASSIC_APP_BUILD) || _MSC_VER >= 1900 // Volume is only supported MSVC2015 and beyond for WinRT
    hr = m_interface->m_client->GetService(IID_PPV_ARGS(&m_volumeControl));
    if (FAILED(hr))
        qCDebug(lcMmAudioInput) << "Could not acquire volume control.";
#endif // defined(CLASSIC_APP_BUILD) || _MSC_VER >= 1900

    hr = m_interface->m_client->GetBufferSize(&m_bufferFrames);
    EMIT_RETURN_FALSE_IF_FAILED("Could not access buffer size.", QAudio::OpenError)

    m_event = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
    m_eventThread = new QWasapiProcessThread(this, false);
    m_eventThread->m_event = m_event;
    hr = m_interface->m_client->SetEventHandle(m_event);
    EMIT_RETURN_FALSE_IF_FAILED("Could not set event handle.", QAudio::OpenError)

    if (!m_pullMode) {
        m_eventDevice = new WasapiInputDevicePrivate(this);
        m_eventDevice->open(QIODevice::ReadOnly|QIODevice::Unbuffered);
    }

    hr = m_interface->m_client->Start();
    EMIT_RETURN_FALSE_IF_FAILED("Could not start audio render client.", QAudio::OpenError)

    m_openTime.restart();
    m_openTimeOffset = 0;
    m_bytesProcessed = 0;

    return true;
}

QAudio::Error QWasapiAudioInput::error() const
{
    qCDebug(lcMmAudioInput) << __FUNCTION__ << m_currentError;
    return m_currentError;
}

QAudio::State QWasapiAudioInput::state() const
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
    return m_currentState;
}

void QWasapiAudioInput::start(QIODevice* device)
{
    qCDebug(lcMmAudioInput) << __FUNCTION__ << device;
    if (!initStart(true)) {
        qCDebug(lcMmAudioInput) << __FUNCTION__ << "failed";
        return;
    }
    m_eventDevice = device;

    m_mutex.lock();
    m_currentState = QAudio::ActiveState;
    m_mutex.unlock();
    emit stateChanged(m_currentState);
    m_eventThread->start();
}

QIODevice *QWasapiAudioInput::start()
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
    if (!initStart(false)) {
        qCDebug(lcMmAudioInput) << __FUNCTION__ << "failed";
        return nullptr;
    }

    m_mutex.lock();
    m_currentState = QAudio::IdleState;
    m_mutex.unlock();
    emit stateChanged(m_currentState);
    m_eventThread->start();
    return m_eventDevice;
}

void QWasapiAudioInput::stop()
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
    if (m_currentState == QAudio::StoppedState)
        return;

    m_mutex.lock();
    m_currentState = QAudio::StoppedState;
    m_mutex.unlock();
    emit stateChanged(m_currentState);

    if (m_currentError != QAudio::NoError) {
        m_mutex.lock();
        m_currentError = QAudio::NoError;
        m_mutex.unlock();
        emit errorChanged(m_currentError);
    }

    HRESULT hr = m_interface->m_client->Stop();
    hr = m_interface->m_client->Reset();

    if (m_eventThread) {
        SetEvent(m_eventThread->m_event);
        while (m_eventThread->isRunning())
            QThread::yieldCurrentThread();
        m_eventThread->deleteLater();
        m_eventThread = 0;
    }
}

void QWasapiAudioInput::setFormat(const QAudioFormat& fmt)
{
    qCDebug(lcMmAudioInput) << __FUNCTION__ << fmt;
    if (m_currentState != QAudio::StoppedState)
        return;
    m_currentFormat = fmt;
}

QAudioFormat QWasapiAudioInput::format() const
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
    return m_currentFormat;
}

int QWasapiAudioInput::bytesReady() const
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
    if (m_currentState != QAudio::IdleState && m_currentState != QAudio::ActiveState)
        return 0;

    const quint32 channelCount = m_currentFormat.channelCount();
    const quint32 sampleBytes = m_currentFormat.sampleSize() / 8;

    quint32 packetFrames;
    HRESULT hr = m_capture->GetNextPacketSize(&packetFrames);

    if (FAILED(hr))
        return 0;
    const quint32 writeBytes = packetFrames * channelCount * sampleBytes;

    return writeBytes;
}

void QWasapiAudioInput::resume()
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
    if (m_currentState != QAudio::SuspendedState)
        return;

    HRESULT hr = m_interface->m_client->Start();
    EMIT_RETURN_VOID_IF_FAILED("Could not start audio render client.", QAudio::FatalError)

    m_mutex.lock();
    m_currentError = QAudio::NoError;
    m_currentState = QAudio::ActiveState;
    m_mutex.unlock();
    emit stateChanged(m_currentState);
    m_eventThread->start();
}

void QWasapiAudioInput::setBufferSize(int value)
{
    qCDebug(lcMmAudioInput) << __FUNCTION__ << value;
    if (m_currentState == QAudio::ActiveState || m_currentState == QAudio::IdleState)
        return;
    m_bufferBytes = value;
}

int QWasapiAudioInput::bufferSize() const
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
    if (m_currentState == QAudio::ActiveState || m_currentState == QAudio::IdleState)
        return m_bufferFrames * m_currentFormat.channelCount()* m_currentFormat.sampleSize() / 8;

    return m_bufferBytes;
}

int QWasapiAudioInput::periodSize() const
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
    REFERENCE_TIME defaultDevicePeriod;
    REFERENCE_TIME minimumPeriod;
    HRESULT hr = m_interface->m_client->GetDevicePeriod(&defaultDevicePeriod, &minimumPeriod);
    if (FAILED(hr))
        return 0;
    const int res = m_currentFormat.bytesForDuration(minimumPeriod / 10);
    return res;
}

void QWasapiAudioInput::setNotifyInterval(int ms)
{
    qCDebug(lcMmAudioInput) << __FUNCTION__ << ms;
    m_interval = qMax(0, ms);
}

int QWasapiAudioInput::notifyInterval() const
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
    return m_interval;
}

qint64 QWasapiAudioInput::processedUSecs() const
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
    if (m_currentState == QAudio::StoppedState)
        return 0;
    qint64 res = qint64(1000000) * m_bytesProcessed /
                 (m_currentFormat.channelCount() * (m_currentFormat.sampleSize() / 8)) /
                 m_currentFormat.sampleRate();

    return res;
}

void QWasapiAudioInput::suspend()
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
    if (m_currentState != QAudio::ActiveState)
        return;

    m_mutex.lock();
    m_currentState = QAudio::SuspendedState;
    m_mutex.unlock();
    emit stateChanged(m_currentState);

    HRESULT hr = m_interface->m_client->Stop();
    EMIT_RETURN_VOID_IF_FAILED("Could not suspend audio render client.", QAudio::FatalError);

    if (m_eventThread) {
        SetEvent(m_eventThread->m_event);
        while (m_eventThread->isRunning())
            QThread::yieldCurrentThread();
    }
    qCDebug(lcMmAudioInput) << __FUNCTION__ << "Thread has stopped";
}

qint64 QWasapiAudioInput::elapsedUSecs() const
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
    if (m_currentState == QAudio::StoppedState)
        return 0;
    return m_openTime.elapsed() * qint64(1000);
}

void QWasapiAudioInput::reset()
{
    qCDebug(lcMmAudioInput) << __FUNCTION__;
    stop();
}

QT_END_NAMESPACE

#include "qwasapiaudioinput.moc"
