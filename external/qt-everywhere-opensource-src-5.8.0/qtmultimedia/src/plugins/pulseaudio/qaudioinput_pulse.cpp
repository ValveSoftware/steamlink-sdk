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

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtCore/qmath.h>
#include <private/qaudiohelpers_p.h>

#include "qaudioinput_pulse.h"
#include "qaudiodeviceinfo_pulse.h"
#include "qpulseaudioengine.h"
#include "qpulsehelpers.h"
#include <sys/types.h>
#include <unistd.h>

QT_BEGIN_NAMESPACE

const int PeriodTimeMs = 50;

static void inputStreamReadCallback(pa_stream *stream, size_t length, void *userdata)
{
    Q_UNUSED(userdata);
    Q_UNUSED(length);
    Q_UNUSED(stream);
    QPulseAudioEngine *pulseEngine = QPulseAudioEngine::instance();
    pa_threaded_mainloop_signal(pulseEngine->mainloop(), 0);
}

static void inputStreamStateCallback(pa_stream *stream, void *userdata)
{
    Q_UNUSED(userdata);
    pa_stream_state_t state = pa_stream_get_state(stream);
#ifdef DEBUG_PULSE
    qDebug() << "Stream state: " << QPulseAudioInternal::stateToQString(state);
#endif
    switch (state) {
        case PA_STREAM_CREATING:
        break;
        case PA_STREAM_READY: {
#ifdef DEBUG_PULSE
            QPulseAudioInput *audioInput = static_cast<QPulseAudioInput*>(userdata);
            const pa_buffer_attr *buffer_attr = pa_stream_get_buffer_attr(stream);
            qDebug() << "*** maxlength: " << buffer_attr->maxlength;
            qDebug() << "*** prebuf: " << buffer_attr->prebuf;
            qDebug() << "*** fragsize: " << buffer_attr->fragsize;
            qDebug() << "*** minreq: " << buffer_attr->minreq;
            qDebug() << "*** tlength: " << buffer_attr->tlength;

            pa_sample_spec spec = QPulseAudioInternal::audioFormatToSampleSpec(audioInput->format());
            qDebug() << "*** bytes_to_usec: " << pa_bytes_to_usec(buffer_attr->fragsize, &spec);
#endif
            }
            break;
        case PA_STREAM_TERMINATED:
            break;
        case PA_STREAM_FAILED:
        default:
            qWarning() << QString("Stream error: %1").arg(pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
            QPulseAudioEngine *pulseEngine = QPulseAudioEngine::instance();
            pa_threaded_mainloop_signal(pulseEngine->mainloop(), 0);
            break;
    }
}

static void inputStreamUnderflowCallback(pa_stream *stream, void *userdata)
{
    Q_UNUSED(userdata)
    Q_UNUSED(stream)
    qWarning() << "Got a buffer underflow!";
}

static void inputStreamOverflowCallback(pa_stream *stream, void *userdata)
{
    Q_UNUSED(stream)
    Q_UNUSED(userdata)
    qWarning() << "Got a buffer overflow!";
}

static void inputStreamSuccessCallback(pa_stream *stream, int success, void *userdata)
{
    Q_UNUSED(stream);
    Q_UNUSED(userdata);
    Q_UNUSED(success);

    //if (!success)
    //TODO: Is cork success?  i->operation_success = success;

    QPulseAudioEngine *pulseEngine = QPulseAudioEngine::instance();
    pa_threaded_mainloop_signal(pulseEngine->mainloop(), 0);
}

QPulseAudioInput::QPulseAudioInput(const QByteArray &device)
    : m_totalTimeValue(0)
    , m_audioSource(0)
    , m_errorState(QAudio::NoError)
    , m_deviceState(QAudio::StoppedState)
    , m_volume(qreal(1.0f))
    , m_pullMode(true)
    , m_opened(false)
    , m_bytesAvailable(0)
    , m_bufferSize(0)
    , m_periodSize(0)
    , m_intervalTime(1000)
    , m_periodTime(PeriodTimeMs)
    , m_stream(0)
    , m_device(device)
{
    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), SLOT(userFeed()));
}

QPulseAudioInput::~QPulseAudioInput()
{
    close();
    disconnect(m_timer, SIGNAL(timeout()));
    QCoreApplication::processEvents();
    delete m_timer;
}

void QPulseAudioInput::setError(QAudio::Error error)
{
    if (m_errorState == error)
        return;

    m_errorState = error;
    emit errorChanged(error);
}

QAudio::Error QPulseAudioInput::error() const
{
    return m_errorState;
}

void QPulseAudioInput::setState(QAudio::State state)
{
    if (m_deviceState == state)
        return;

    m_deviceState = state;
    emit stateChanged(state);
}

QAudio::State QPulseAudioInput::state() const
{
    return m_deviceState;
}

void QPulseAudioInput::setFormat(const QAudioFormat &format)
{
    if (m_deviceState == QAudio::StoppedState)
        m_format = format;
}

QAudioFormat QPulseAudioInput::format() const
{
    return m_format;
}

void QPulseAudioInput::start(QIODevice *device)
{
    setState(QAudio::StoppedState);
    setError(QAudio::NoError);

    if (!m_pullMode && m_audioSource) {
        delete m_audioSource;
        m_audioSource = 0;
    }

    close();

    if (!open())
        return;

    m_pullMode = true;
    m_audioSource = device;

    setState(QAudio::ActiveState);
}

QIODevice *QPulseAudioInput::start()
{
    setState(QAudio::StoppedState);
    setError(QAudio::NoError);

    if (!m_pullMode && m_audioSource) {
        delete m_audioSource;
        m_audioSource = 0;
    }

    close();

    if (!open())
        return Q_NULLPTR;

    m_pullMode = false;
    m_audioSource = new PulseInputPrivate(this);
    m_audioSource->open(QIODevice::ReadOnly | QIODevice::Unbuffered);

    setState(QAudio::IdleState);

    return m_audioSource;
}

void QPulseAudioInput::stop()
{
    if (m_deviceState == QAudio::StoppedState)
        return;

    close();

    setError(QAudio::NoError);
    setState(QAudio::StoppedState);
}

bool QPulseAudioInput::open()
{
    if (m_opened)
        return true;

    QPulseAudioEngine *pulseEngine = QPulseAudioEngine::instance();

    if (!pulseEngine->context() || pa_context_get_state(pulseEngine->context()) != PA_CONTEXT_READY) {
        setError(QAudio::FatalError);
        setState(QAudio::StoppedState);
        return false;
    }

    pa_sample_spec spec = QPulseAudioInternal::audioFormatToSampleSpec(m_format);

    if (!pa_sample_spec_valid(&spec)) {
        setError(QAudio::OpenError);
        setState(QAudio::StoppedState);
        return false;
    }

    m_spec = spec;

#ifdef DEBUG_PULSE
//    QTime now(QTime::currentTime());
//    qDebug()<<now.second()<<"s "<<now.msec()<<"ms :open()";
#endif

    if (m_streamName.isNull())
        m_streamName = QString(QLatin1String("QtmPulseStream-%1-%2")).arg(::getpid()).arg(quintptr(this)).toUtf8();

#ifdef DEBUG_PULSE
        qDebug() << "Format: " << QPulseAudioInternal::sampleFormatToQString(spec.format);
        qDebug() << "Rate: " << spec.rate;
        qDebug() << "Channels: " << spec.channels;
        qDebug() << "Frame size: " << pa_frame_size(&spec);
#endif

    pulseEngine->lock();
    pa_channel_map channel_map;

    pa_channel_map_init_extend(&channel_map, spec.channels, PA_CHANNEL_MAP_DEFAULT);

    if (!pa_channel_map_compatible(&channel_map, &spec))
        qWarning() << "Channel map doesn't match sample specification!";

    m_stream = pa_stream_new(pulseEngine->context(), m_streamName.constData(), &spec, &channel_map);

    pa_stream_set_state_callback(m_stream, inputStreamStateCallback, this);
    pa_stream_set_read_callback(m_stream, inputStreamReadCallback, this);

    pa_stream_set_underflow_callback(m_stream, inputStreamUnderflowCallback, this);
    pa_stream_set_overflow_callback(m_stream, inputStreamOverflowCallback, this);

    m_periodSize = pa_usec_to_bytes(PeriodTimeMs*1000, &spec);

    int flags = 0;
    pa_buffer_attr buffer_attr;
    buffer_attr.maxlength = (uint32_t) -1;
    buffer_attr.prebuf = (uint32_t) -1;
    buffer_attr.tlength = (uint32_t) -1;
    buffer_attr.minreq = (uint32_t) -1;
    flags |= PA_STREAM_ADJUST_LATENCY;

    if (m_bufferSize > 0)
        buffer_attr.fragsize = (uint32_t) m_bufferSize;
    else
        buffer_attr.fragsize = (uint32_t) m_periodSize;

    if (pa_stream_connect_record(m_stream, m_device.data(), &buffer_attr, (pa_stream_flags_t)flags) < 0) {
        qWarning() << "pa_stream_connect_record() failed!";
        pa_stream_unref(m_stream);
        m_stream = 0;
        pulseEngine->unlock();
        setError(QAudio::OpenError);
        setState(QAudio::StoppedState);
        return false;
    }

    while (pa_stream_get_state(m_stream) != PA_STREAM_READY)
        pa_threaded_mainloop_wait(pulseEngine->mainloop());

    const pa_buffer_attr *actualBufferAttr = pa_stream_get_buffer_attr(m_stream);
    m_periodSize = actualBufferAttr->fragsize;
    m_periodTime = pa_bytes_to_usec(m_periodSize, &spec) / 1000;
    if (actualBufferAttr->tlength != (uint32_t)-1)
        m_bufferSize = actualBufferAttr->tlength;

    pulseEngine->unlock();

    connect(pulseEngine, &QPulseAudioEngine::contextFailed, this, &QPulseAudioInput::onPulseContextFailed);

    m_opened = true;
    m_timer->start(m_periodTime);

    m_clockStamp.restart();
    m_timeStamp.restart();
    m_elapsedTimeOffset = 0;
    m_totalTimeValue = 0;

    return true;
}

void QPulseAudioInput::close()
{
    if (!m_opened)
        return;

    m_timer->stop();

    QPulseAudioEngine *pulseEngine = QPulseAudioEngine::instance();

    if (m_stream) {
        pulseEngine->lock();

        pa_stream_set_state_callback(m_stream, 0, 0);
        pa_stream_set_read_callback(m_stream, 0, 0);
        pa_stream_set_underflow_callback(m_stream, 0, 0);
        pa_stream_set_overflow_callback(m_stream, 0, 0);

        pa_stream_disconnect(m_stream);
        pa_stream_unref(m_stream);
        m_stream = 0;

        pulseEngine->unlock();
    }

    disconnect(pulseEngine, &QPulseAudioEngine::contextFailed, this, &QPulseAudioInput::onPulseContextFailed);

    if (!m_pullMode && m_audioSource) {
        delete m_audioSource;
        m_audioSource = 0;
    }
    m_opened = false;
}

int QPulseAudioInput::checkBytesReady()
{
    if (m_deviceState != QAudio::ActiveState && m_deviceState != QAudio::IdleState) {
        m_bytesAvailable = 0;
    } else {
        m_bytesAvailable = pa_stream_readable_size(m_stream);
    }

    return m_bytesAvailable;
}

int QPulseAudioInput::bytesReady() const
{
    return qMax(m_bytesAvailable, 0);
}

qint64 QPulseAudioInput::read(char *data, qint64 len)
{
    m_bytesAvailable = checkBytesReady();

    setError(QAudio::NoError);
    setState(QAudio::ActiveState);

    int readBytes = 0;

    if (!m_pullMode && !m_tempBuffer.isEmpty()) {
        readBytes = qMin(static_cast<int>(len), m_tempBuffer.size());
        memcpy(data, m_tempBuffer.constData(), readBytes);
        m_totalTimeValue += readBytes;

        if (readBytes < m_tempBuffer.size()) {
            m_tempBuffer.remove(0, readBytes);
            return readBytes;
        }

        m_tempBuffer.clear();
    }

    while (pa_stream_readable_size(m_stream) > 0) {
        size_t readLength = 0;

#ifdef DEBUG_PULSE
        qDebug() << "QPulseAudioInput::read -- " << pa_stream_readable_size(m_stream) << " bytes available from pulse audio";
#endif

        QPulseAudioEngine *pulseEngine = QPulseAudioEngine::instance();
        pulseEngine->lock();

        const void *audioBuffer;

        // Second and third parameters (audioBuffer and length) to pa_stream_peek are output parameters,
        // the audioBuffer pointer is set to point to the actual pulse audio data,
        // and the length is set to the length of this data.
        if (pa_stream_peek(m_stream, &audioBuffer, &readLength) < 0) {
            qWarning() << QString("pa_stream_peek() failed: %1").arg(pa_strerror(pa_context_errno(pa_stream_get_context(m_stream))));
            pulseEngine->unlock();
            return 0;
        }

        qint64 actualLength = 0;
        if (m_pullMode) {
            QByteArray adjusted(readLength, Qt::Uninitialized);
            applyVolume(audioBuffer, adjusted.data(), readLength);
            actualLength = m_audioSource->write(adjusted);

            if (actualLength < qint64(readLength)) {
                pulseEngine->unlock();

                setError(QAudio::UnderrunError);
                setState(QAudio::IdleState);

                return actualLength;
            }
        } else {
            actualLength = qMin(static_cast<int>(len - readBytes), static_cast<int>(readLength));
            applyVolume(audioBuffer, data + readBytes, actualLength);
        }

#ifdef DEBUG_PULSE
        qDebug() << "QPulseAudioInput::read -- wrote " << actualLength << " to client";
#endif

        if (actualLength < qint64(readLength)) {
#ifdef DEBUG_PULSE
            qDebug() << "QPulseAudioInput::read -- appending " << readLength - actualLength << " bytes of data to temp buffer";
#endif
            int diff = readLength - actualLength;
            int oldSize = m_tempBuffer.size();
            m_tempBuffer.resize(m_tempBuffer.size() + diff);
            applyVolume(static_cast<const char *>(audioBuffer) + actualLength, m_tempBuffer.data() + oldSize, diff);
            QMetaObject::invokeMethod(this, "userFeed", Qt::QueuedConnection);
        }

        m_totalTimeValue += actualLength;
        readBytes += actualLength;

        pa_stream_drop(m_stream);
        pulseEngine->unlock();

        if (!m_pullMode && readBytes >= len)
            break;

        if (m_intervalTime && (m_timeStamp.elapsed() + m_elapsedTimeOffset) > m_intervalTime) {
            emit notify();
            m_elapsedTimeOffset = m_timeStamp.elapsed() + m_elapsedTimeOffset - m_intervalTime;
            m_timeStamp.restart();
        }
    }

#ifdef DEBUG_PULSE
    qDebug() << "QPulseAudioInput::read -- returning after reading " << readBytes << " bytes";
#endif

    return readBytes;
}

void QPulseAudioInput::applyVolume(const void *src, void *dest, int len)
{
    if (m_volume < 1.f)
        QAudioHelperInternal::qMultiplySamples(m_volume, m_format, src, dest, len);
    else
        memcpy(dest, src, len);
}

void QPulseAudioInput::resume()
{
    if (m_deviceState == QAudio::SuspendedState || m_deviceState == QAudio::IdleState) {
        QPulseAudioEngine *pulseEngine = QPulseAudioEngine::instance();
        pa_operation *operation;

        pulseEngine->lock();

        operation = pa_stream_cork(m_stream, 0, inputStreamSuccessCallback, 0);
        pulseEngine->wait(operation);
        pa_operation_unref(operation);

        pulseEngine->unlock();

        m_timer->start(m_periodTime);

        setState(QAudio::ActiveState);
        setError(QAudio::NoError);
    }
}

void QPulseAudioInput::setVolume(qreal vol)
{
    if (qFuzzyCompare(m_volume, vol))
        return;

    m_volume = qBound(qreal(0), vol, qreal(1));
}

qreal QPulseAudioInput::volume() const
{
    return m_volume;
}

void QPulseAudioInput::setBufferSize(int value)
{
    m_bufferSize = value;
}

int QPulseAudioInput::bufferSize() const
{
    return m_bufferSize;
}

int QPulseAudioInput::periodSize() const
{
    return m_periodSize;
}

void QPulseAudioInput::setNotifyInterval(int ms)
{
    m_intervalTime = qMax(0, ms);
}

int QPulseAudioInput::notifyInterval() const
{
    return m_intervalTime;
}

qint64 QPulseAudioInput::processedUSecs() const
{
    pa_sample_spec spec = QPulseAudioInternal::audioFormatToSampleSpec(m_format);
    qint64 result = pa_bytes_to_usec(m_totalTimeValue, &spec);

    return result;
}

void QPulseAudioInput::suspend()
{
    if (m_deviceState == QAudio::ActiveState) {
        setError(QAudio::NoError);
        setState(QAudio::SuspendedState);

        m_timer->stop();

        QPulseAudioEngine *pulseEngine = QPulseAudioEngine::instance();
        pa_operation *operation;

        pulseEngine->lock();

        operation = pa_stream_cork(m_stream, 1, inputStreamSuccessCallback, 0);
        pulseEngine->wait(operation);
        pa_operation_unref(operation);

        pulseEngine->unlock();
    }
}

void QPulseAudioInput::userFeed()
{
    if (m_deviceState == QAudio::StoppedState || m_deviceState == QAudio::SuspendedState)
        return;
#ifdef DEBUG_PULSE
//    QTime now(QTime::currentTime());
//    qDebug()<< now.second() << "s " << now.msec() << "ms :userFeed() IN";
#endif
    deviceReady();
}

bool QPulseAudioInput::deviceReady()
{
   if (m_pullMode) {
        // reads some audio data and writes it to QIODevice
        read(0,0);
    } else {
        // emits readyRead() so user will call read() on QIODevice to get some audio data
        if (m_audioSource != 0) {
            PulseInputPrivate *a = qobject_cast<PulseInputPrivate*>(m_audioSource);
            a->trigger();
        }
    }
    m_bytesAvailable = checkBytesReady();

    if (m_deviceState != QAudio::ActiveState)
        return true;

    if (m_intervalTime && (m_timeStamp.elapsed() + m_elapsedTimeOffset) > m_intervalTime) {
        emit notify();
        m_elapsedTimeOffset = m_timeStamp.elapsed() + m_elapsedTimeOffset - m_intervalTime;
        m_timeStamp.restart();
    }

    return true;
}

qint64 QPulseAudioInput::elapsedUSecs() const
{
    if (m_deviceState == QAudio::StoppedState)
        return 0;

    return m_clockStamp.elapsed() * qint64(1000);
}

void QPulseAudioInput::reset()
{
    stop();
    m_bytesAvailable = 0;
}

void QPulseAudioInput::onPulseContextFailed()
{
    close();

    setError(QAudio::FatalError);
    setState(QAudio::StoppedState);
}

PulseInputPrivate::PulseInputPrivate(QPulseAudioInput *audio)
{
    m_audioDevice = qobject_cast<QPulseAudioInput*>(audio);
}

qint64 PulseInputPrivate::readData(char *data, qint64 len)
{
    return m_audioDevice->read(data, len);
}

qint64 PulseInputPrivate::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data)
    Q_UNUSED(len)
    return 0;
}

void PulseInputPrivate::trigger()
{
    emit readyRead();
}

QT_END_NAMESPACE

#include "moc_qaudioinput_pulse.cpp"
