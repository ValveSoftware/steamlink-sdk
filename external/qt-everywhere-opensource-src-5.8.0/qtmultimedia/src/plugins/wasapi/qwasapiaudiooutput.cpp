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

#include "qwasapiaudiooutput.h"
#include "qwasapiutils.h"
#include <QtCore/qfunctions_winrt.h>
#include <QtCore/QMutexLocker>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <Audioclient.h>
#include <functional>

using namespace Microsoft::WRL;

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcMmAudioOutput, "qt.multimedia.audiooutput")

class WasapiOutputDevicePrivate : public QIODevice
{
    Q_OBJECT
public:
    WasapiOutputDevicePrivate(QWasapiAudioOutput* output);
    ~WasapiOutputDevicePrivate();

    qint64 readData(char* data, qint64 len);
    qint64 writeData(const char* data, qint64 len);

private:
    QWasapiAudioOutput *m_output;
    QTimer m_timer;
};

WasapiOutputDevicePrivate::WasapiOutputDevicePrivate(QWasapiAudioOutput* output)
    : m_output(output)
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;

    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, [=](){
        if (m_output->m_currentState == QAudio::ActiveState) {
            m_output->m_currentState = QAudio::IdleState;
            emit m_output->stateChanged(m_output->m_currentState);
            m_output->m_currentError = QAudio::UnderrunError;
            emit m_output->errorChanged(m_output->m_currentError);
        }
        });
}

WasapiOutputDevicePrivate::~WasapiOutputDevicePrivate()
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
}

qint64 WasapiOutputDevicePrivate::readData(char* data, qint64 len)
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    Q_UNUSED(data)
    Q_UNUSED(len)

    return 0;
}

qint64 WasapiOutputDevicePrivate::writeData(const char* data, qint64 len)
{
    if (m_output->state() != QAudio::ActiveState && m_output->state() != QAudio::IdleState)
        return 0;

    QMutexLocker locker(&m_output->m_mutex);
    m_timer.stop();

    const quint32 channelCount = m_output->m_currentFormat.channelCount();
    const quint32 sampleBytes = m_output->m_currentFormat.sampleSize() / 8;
    const quint32 freeBytes = static_cast<quint32>(m_output->bytesFree());
    const quint32 bytesToWrite = qMin(freeBytes, static_cast<quint32>(len));
    const quint32 framesToWrite = bytesToWrite / (channelCount * sampleBytes);

    BYTE *buffer;
    HRESULT hr;
    hr = m_output->m_renderer->GetBuffer(framesToWrite, &buffer);
    if (hr != S_OK) {
        m_output->m_currentError = QAudio::UnderrunError;
        QMetaObject::invokeMethod(m_output, "errorChanged", Qt::QueuedConnection,
                                  Q_ARG(QAudio::Error, QAudio::UnderrunError));
        // Also Error Buffers need to be released
        hr = m_output->m_renderer->ReleaseBuffer(framesToWrite, 0);
        return 0;
    }

    memcpy_s(buffer, bytesToWrite, data, bytesToWrite);

    hr = m_output->m_renderer->ReleaseBuffer(framesToWrite, 0);
    if (hr != S_OK)
        qFatal("Could not release buffer");

    if (m_output->m_interval && m_output->m_openTime.elapsed() - m_output->m_openTimeOffset > m_output->m_interval) {
        QMetaObject::invokeMethod(m_output, "notify", Qt::QueuedConnection);
        m_output->m_openTimeOffset = m_output->m_openTime.elapsed();
    }

    m_output->m_bytesProcessed += bytesToWrite;

    if (m_output->m_currentState != QAudio::ActiveState) {
        m_output->m_currentState = QAudio::ActiveState;
        emit m_output->stateChanged(m_output->m_currentState);
    }
    if (m_output->m_currentError != QAudio::NoError) {
        m_output->m_currentError = QAudio::NoError;
        emit m_output->errorChanged(m_output->m_currentError);
    }

    quint32 paddingFrames;
    hr = m_output->m_interface->m_client->GetCurrentPadding(&paddingFrames);
    const quint32 paddingBytes = paddingFrames * m_output->m_currentFormat.channelCount() * m_output->m_currentFormat.sampleSize() / 8;

    m_timer.setInterval(m_output->m_currentFormat.durationForBytes(paddingBytes) / 1000);
    m_timer.start();
    return bytesToWrite;
}

QWasapiAudioOutput::QWasapiAudioOutput(const QByteArray &device)
    : m_deviceName(device)
#if defined(CLASSIC_APP_BUILD) || _MSC_VER >= 1900
    , m_volumeCache(qreal(1.))
#endif
    , m_currentState(QAudio::StoppedState)
    , m_currentError(QAudio::NoError)
    , m_interval(1000)
    , m_pullMode(true)
    , m_bufferFrames(0)
    , m_bufferBytes(4096)
    , m_eventThread(0)
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__ << device;
}

QWasapiAudioOutput::~QWasapiAudioOutput()
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    stop();
}

void QWasapiAudioOutput::setVolume(qreal vol)
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__ << vol;
#if defined(CLASSIC_APP_BUILD) || _MSC_VER >= 1900 // Volume is only supported MSVC2015 and beyond for WinRT
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

qreal QWasapiAudioOutput::volume() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
#if defined(CLASSIC_APP_BUILD) || _MSC_VER >= 1900 // Volume is only supported MSVC2015 and beyond for WinRT
    return m_volumeCache;
#else
    return qreal(1.0);
#endif
}

void QWasapiAudioOutput::process()
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
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

void QWasapiAudioOutput::processBuffer()
{
    QMutexLocker locker(&m_mutex);

    const quint32 channelCount = m_currentFormat.channelCount();
    const quint32 sampleBytes = m_currentFormat.sampleSize() / 8;
    BYTE* buffer;
    HRESULT hr;

    quint32 paddingFrames;
    hr = m_interface->m_client->GetCurrentPadding(&paddingFrames);

    const quint32 availableFrames = m_bufferFrames - paddingFrames;
    hr = m_renderer->GetBuffer(availableFrames, &buffer);
    if (hr != S_OK) {
        m_currentError = QAudio::UnderrunError;
        emit errorChanged(m_currentError);
        // Also Error Buffers need to be released
        hr = m_renderer->ReleaseBuffer(availableFrames, 0);
        ResetEvent(m_event);
        return;
    }

    const quint32 readBytes = availableFrames * channelCount * sampleBytes;
    const qint64 read = m_eventDevice->read((char*)buffer, readBytes);
    if (read < static_cast<qint64>(readBytes)) {
        // Fill the rest of the buffer with zero to avoid audio glitches
        if (m_currentError != QAudio::UnderrunError) {
            m_currentError = QAudio::UnderrunError;
            emit errorChanged(m_currentError);
        }
        if (m_currentState != QAudio::IdleState) {
            m_currentState = QAudio::IdleState;
            emit stateChanged(m_currentState);
        }
    }

    hr = m_renderer->ReleaseBuffer(availableFrames, 0);
    if (hr != S_OK)
        qFatal("Could not release buffer");
    ResetEvent(m_event);

    if (m_interval && m_openTime.elapsed() - m_openTimeOffset > m_interval) {
        emit notify();
        m_openTimeOffset = m_openTime.elapsed();
    }

    m_bytesProcessed += read;
    m_processing = m_currentState == QAudio::ActiveState || m_currentState == QAudio::IdleState;
}

bool QWasapiAudioOutput::initStart(bool pull)
{
    if (m_currentState == QAudio::ActiveState || m_currentState == QAudio::IdleState)
        stop();

    QMutexLocker locker(&m_mutex);

    m_interface = QWasapiUtils::createOrGetInterface(m_deviceName, QAudio::AudioOutput);
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
        t = m_currentFormat.durationForBytes(m_bufferBytes) * 100;

    DWORD flags = pull ? AUDCLNT_STREAMFLAGS_EVENTCALLBACK : 0;
    hr = m_interface->m_client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, t, 0, &nFmt, NULL);
    EMIT_RETURN_FALSE_IF_FAILED("Could not initialize audio client.", QAudio::OpenError)

    hr = m_interface->m_client->GetService(IID_PPV_ARGS(&m_renderer));
    EMIT_RETURN_FALSE_IF_FAILED("Could not acquire render service.", QAudio::OpenError)

#if defined(CLASSIC_APP_BUILD) || _MSC_VER >= 1900 // Volume is only supported MSVC2015 and beyond
    hr = m_interface->m_client->GetService(IID_PPV_ARGS(&m_volumeControl));
    if (FAILED(hr))
        qCDebug(lcMmAudioOutput) << "Could not acquire volume control.";
#endif // defined(CLASSIC_APP_BUILD) || _MSC_VER >= 1900

    hr = m_interface->m_client->GetBufferSize(&m_bufferFrames);
    EMIT_RETURN_FALSE_IF_FAILED("Could not access buffer size.", QAudio::OpenError)

    if (m_pullMode) {
        m_eventThread = new QWasapiProcessThread(this);
        m_event = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
        m_eventThread->m_event = m_event;

        hr = m_interface->m_client->SetEventHandle(m_event);
        EMIT_RETURN_FALSE_IF_FAILED("Could not set event handle.", QAudio::OpenError)
    } else {
        m_eventDevice = new WasapiOutputDevicePrivate(this);
        m_eventDevice->open(QIODevice::WriteOnly|QIODevice::Unbuffered);
    }
    // Send some initial data, do not exit on failure, latest in process
    // those an error will be caught
    BYTE *pdata = nullptr;
    hr = m_renderer->GetBuffer(m_bufferFrames, &pdata);
    hr = m_renderer->ReleaseBuffer(m_bufferFrames, AUDCLNT_BUFFERFLAGS_SILENT);

    hr = m_interface->m_client->Start();
    EMIT_RETURN_FALSE_IF_FAILED("Could not start audio render client.", QAudio::OpenError)

    m_openTime.restart();
    m_openTimeOffset = 0;
    m_bytesProcessed = 0;
    return true;
}

QAudio::Error QWasapiAudioOutput::error() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__ << m_currentError;
    return m_currentError;
}

QAudio::State QWasapiAudioOutput::state() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    return m_currentState;
}

void QWasapiAudioOutput::start(QIODevice *device)
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__ << device;
    if (!initStart(true)) {
        qCDebug(lcMmAudioOutput) << __FUNCTION__ << "failed";
        return;
    }
    m_eventDevice = device;

    m_mutex.lock();
    m_currentState = QAudio::ActiveState;
    m_mutex.unlock();
    emit stateChanged(m_currentState);
    m_eventThread->start();
}

QIODevice *QWasapiAudioOutput::start()
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;

    if (!initStart(false)) {
        qCDebug(lcMmAudioOutput) << __FUNCTION__ << "failed";
        return nullptr;
    }

    m_mutex.lock();
    m_currentState = QAudio::IdleState;
    m_mutex.unlock();
    emit stateChanged(m_currentState);

    return m_eventDevice;
}

void QWasapiAudioOutput::stop()
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    if (m_currentState == QAudio::StoppedState)
        return;

    if (!m_pullMode) {
        HRESULT hr;
        hr = m_interface->m_client->Stop();
        hr = m_interface->m_client->Reset();
    }

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
    if (m_currentState == QAudio::StoppedState) {
        hr = m_interface->m_client->Reset();
    }

    if (m_eventThread) {
        SetEvent(m_eventThread->m_event);
        while (m_eventThread->isRunning())
            QThread::yieldCurrentThread();
        m_eventThread->deleteLater();
        m_eventThread = 0;
    }
}

int QWasapiAudioOutput::bytesFree() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    if (m_currentState != QAudio::ActiveState && m_currentState != QAudio::IdleState)
        return 0;

    quint32 paddingFrames;
    HRESULT hr = m_interface->m_client->GetCurrentPadding(&paddingFrames);
    if (FAILED(hr)) {
        qCDebug(lcMmAudioOutput) << __FUNCTION__ << "Could not query padding frames.";
        return bufferSize();
    }

    const quint32 availableFrames = m_bufferFrames - paddingFrames;
    const quint32 res = availableFrames * m_currentFormat.channelCount() * m_currentFormat.sampleSize() / 8;
    return res;
}

int QWasapiAudioOutput::periodSize() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    REFERENCE_TIME defaultDevicePeriod;
    HRESULT hr = m_interface->m_client->GetDevicePeriod(&defaultDevicePeriod, NULL);
    if (FAILED(hr))
        return 0;
    const QAudioFormat f = m_currentFormat.isValid() ? m_currentFormat : m_interface->m_mixFormat;
    const int res = m_currentFormat.bytesForDuration(defaultDevicePeriod / 10);
    return res;
}

void QWasapiAudioOutput::setBufferSize(int value)
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__ << value;
    if (m_currentState == QAudio::ActiveState || m_currentState == QAudio::IdleState)
        return;
    m_bufferBytes = value;
}

int QWasapiAudioOutput::bufferSize() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    if (m_currentState == QAudio::ActiveState || m_currentState == QAudio::IdleState)
        return m_bufferFrames * m_currentFormat.channelCount()* m_currentFormat.sampleSize() / 8;

    return m_bufferBytes;
}

void QWasapiAudioOutput::setNotifyInterval(int ms)
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__ << ms;
    m_interval = qMax(0, ms);
}

int QWasapiAudioOutput::notifyInterval() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    return m_interval;
}

qint64 QWasapiAudioOutput::processedUSecs() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    if (m_currentState == QAudio::StoppedState)
        return 0;
    qint64 res = qint64(1000000) * m_bytesProcessed /
                 (m_currentFormat.channelCount() * (m_currentFormat.sampleSize() / 8)) /
                 m_currentFormat.sampleRate();

    return res;
}

void QWasapiAudioOutput::resume()
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;

    if (m_currentState != QAudio::SuspendedState)
        return;

    HRESULT hr = m_interface->m_client->Start();
    EMIT_RETURN_VOID_IF_FAILED("Could not start audio render client.", QAudio::FatalError)

    m_mutex.lock();
    m_currentError = QAudio::NoError;
    m_currentState = m_pullMode ? QAudio::ActiveState : QAudio::IdleState;
    m_mutex.unlock();
    emit stateChanged(m_currentState);
    if (m_eventThread)
        m_eventThread->start();
}

void QWasapiAudioOutput::setFormat(const QAudioFormat& fmt)
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__ << fmt;
    if (m_currentState != QAudio::StoppedState)
        return;
    m_currentFormat = fmt;
}

QAudioFormat QWasapiAudioOutput::format() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    return m_currentFormat;
}

void QWasapiAudioOutput::suspend()
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;

    if (m_currentState != QAudio::ActiveState && m_currentState != QAudio::IdleState)
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
        qCDebug(lcMmAudioOutput) << __FUNCTION__ << "Thread has stopped";
    }
}

qint64 QWasapiAudioOutput::elapsedUSecs() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    if (m_currentState == QAudio::StoppedState)
        return 0;
    return m_openTime.elapsed() * qint64(1000);
}

void QWasapiAudioOutput::reset()
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    stop();
}

QT_END_NAMESPACE

#include "qwasapiaudiooutput.moc"
