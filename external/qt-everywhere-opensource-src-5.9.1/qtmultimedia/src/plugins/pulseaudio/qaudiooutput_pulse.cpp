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

#include "qaudiooutput_pulse.h"
#include "qaudiodeviceinfo_pulse.h"
#include "qpulseaudioengine.h"
#include "qpulsehelpers.h"
#include <sys/types.h>
#include <unistd.h>

QT_BEGIN_NAMESPACE

const int PeriodTimeMs = 20;
const int LowLatencyPeriodTimeMs = 10;
const int LowLatencyBufferSizeMs = 40;

#define LOW_LATENCY_CATEGORY_NAME "game"

static void  outputStreamWriteCallback(pa_stream *stream, size_t length, void *userdata)
{
    Q_UNUSED(stream);
    Q_UNUSED(length);
    Q_UNUSED(userdata);
    QPulseAudioEngine *pulseEngine = QPulseAudioEngine::instance();
    pa_threaded_mainloop_signal(pulseEngine->mainloop(), 0);
}

static void outputStreamStateCallback(pa_stream *stream, void *userdata)
{
    Q_UNUSED(userdata)
    pa_stream_state_t state = pa_stream_get_state(stream);
#ifdef DEBUG_PULSE
    qDebug() << "Stream state: " << QPulseAudioInternal::stateToQString(state);
#endif
    switch (state) {
        case PA_STREAM_CREATING:
        case PA_STREAM_READY:
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

static void outputStreamUnderflowCallback(pa_stream *stream, void *userdata)
{
    Q_UNUSED(stream)
    ((QPulseAudioOutput*)userdata)->streamUnderflowCallback();
}

static void outputStreamOverflowCallback(pa_stream *stream, void *userdata)
{
    Q_UNUSED(stream)
    Q_UNUSED(userdata)
    qWarning() << "Got a buffer overflow!";
}

static void outputStreamLatencyCallback(pa_stream *stream, void *userdata)
{
    Q_UNUSED(stream)
    Q_UNUSED(userdata)

#ifdef DEBUG_PULSE
    const pa_timing_info *info = pa_stream_get_timing_info(stream);

    qDebug() << "Write index corrupt: " << info->write_index_corrupt;
    qDebug() << "Write index: " << info->write_index;
    qDebug() << "Read index corrupt: " << info->read_index_corrupt;
    qDebug() << "Read index: " << info->read_index;
    qDebug() << "Sink usec: " << info->sink_usec;
    qDebug() << "Configured sink usec: " << info->configured_sink_usec;
#endif
}

static void outputStreamSuccessCallback(pa_stream *stream, int success, void *userdata)
{
    Q_UNUSED(stream);
    Q_UNUSED(success);
    Q_UNUSED(userdata);

    QPulseAudioEngine *pulseEngine = QPulseAudioEngine::instance();
    pa_threaded_mainloop_signal(pulseEngine->mainloop(), 0);
}

static void outputStreamDrainComplete(pa_stream *stream, int success, void *userdata)
{
    Q_UNUSED(stream);
    Q_UNUSED(success);
    Q_UNUSED(userdata);

#ifdef DEBUG_PULSE
    qDebug() << "Draining completed successfully: " << (bool)success;
#endif
}

static void streamAdjustPrebufferCallback(pa_stream *stream, int success, void *userdata)
{
    Q_UNUSED(stream);
    Q_UNUSED(success);
    Q_UNUSED(userdata);

#ifdef DEBUG_PULSE
    qDebug() << "Adjust prebuffer completed successfully: " << (bool)success;
#endif
}


QPulseAudioOutput::QPulseAudioOutput(const QByteArray &device)
    : m_device(device)
    , m_errorState(QAudio::NoError)
    , m_deviceState(QAudio::StoppedState)
    , m_pullMode(true)
    , m_opened(false)
    , m_audioSource(0)
    , m_periodTime(0)
    , m_stream(0)
    , m_notifyInterval(1000)
    , m_periodSize(0)
    , m_bufferSize(0)
    , m_maxBufferSize(0)
    , m_totalTimeValue(0)
    , m_tickTimer(new QTimer(this))
    , m_audioBuffer(0)
    , m_resuming(false)
    , m_volume(1.0)
{
    connect(m_tickTimer, SIGNAL(timeout()), SLOT(userFeed()));
}

QPulseAudioOutput::~QPulseAudioOutput()
{
    close();
    disconnect(m_tickTimer, SIGNAL(timeout()));
    QCoreApplication::processEvents();
}

void QPulseAudioOutput::setError(QAudio::Error error)
{
    if (m_errorState == error)
        return;

    m_errorState = error;
    emit errorChanged(error);
}

QAudio::Error QPulseAudioOutput::error() const
{
    return m_errorState;
}

void QPulseAudioOutput::setState(QAudio::State state)
{
    if (m_deviceState == state)
        return;

    m_deviceState = state;
    emit stateChanged(state);
}

QAudio::State QPulseAudioOutput::state() const
{
    return m_deviceState;
}

void QPulseAudioOutput::streamUnderflowCallback()
{
    if (m_deviceState != QAudio::IdleState && !m_resuming) {
        setError(QAudio::UnderrunError);
        setState(QAudio::IdleState);
    }
}

void QPulseAudioOutput::start(QIODevice *device)
{
    setState(QAudio::StoppedState);
    setError(QAudio::NoError);

    // Handle change of mode
    if (m_audioSource && !m_pullMode) {
        delete m_audioSource;
    }
    m_audioSource = 0;

    close();

    m_pullMode = true;
    m_audioSource = device;

    if (!open()) {
        m_audioSource = 0;
        return;
    }

    setState(QAudio::ActiveState);
}

QIODevice *QPulseAudioOutput::start()
{
    setState(QAudio::StoppedState);
    setError(QAudio::NoError);

    // Handle change of mode
    if (m_audioSource && !m_pullMode) {
        delete m_audioSource;
    }
    m_audioSource = 0;

    close();

    m_pullMode = false;

    if (!open())
        return Q_NULLPTR;

    m_audioSource = new PulseOutputPrivate(this);
    m_audioSource->open(QIODevice::WriteOnly|QIODevice::Unbuffered);

    setState(QAudio::IdleState);

    return m_audioSource;
}

bool QPulseAudioOutput::open()
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
    m_totalTimeValue = 0;

    if (m_streamName.isNull())
        m_streamName = QString(QLatin1String("QtmPulseStream-%1-%2")).arg(::getpid()).arg(quintptr(this)).toUtf8();

#ifdef DEBUG_PULSE
    qDebug() << "Format: " << QPulseAudioInternal::sampleFormatToQString(spec.format);
    qDebug() << "Rate: " << spec.rate;
    qDebug() << "Channels: " << spec.channels;
    qDebug() << "Frame size: " << pa_frame_size(&spec);
#endif

    pulseEngine->lock();

    qint64 bytesPerSecond = m_format.sampleRate() * m_format.channelCount() * m_format.sampleSize() / 8;

    pa_proplist *propList = pa_proplist_new();
    if (!m_category.isNull())
        pa_proplist_sets(propList, PA_PROP_MEDIA_ROLE, m_category.toLatin1().constData());

    m_stream = pa_stream_new_with_proplist(pulseEngine->context(), m_streamName.constData(), &m_spec, 0, propList);
    pa_proplist_free(propList);

    pa_stream_set_state_callback(m_stream, outputStreamStateCallback, this);
    pa_stream_set_write_callback(m_stream, outputStreamWriteCallback, this);

    pa_stream_set_underflow_callback(m_stream, outputStreamUnderflowCallback, this);
    pa_stream_set_overflow_callback(m_stream, outputStreamOverflowCallback, this);
    pa_stream_set_latency_update_callback(m_stream, outputStreamLatencyCallback, this);

    if (m_bufferSize <= 0 && m_category == LOW_LATENCY_CATEGORY_NAME) {
        m_bufferSize = bytesPerSecond * LowLatencyBufferSizeMs / qint64(1000);
    }

    pa_buffer_attr requestedBuffer;
    requestedBuffer.fragsize = (uint32_t)-1;
    requestedBuffer.maxlength = (uint32_t)-1;
    requestedBuffer.minreq = (uint32_t)-1;
    requestedBuffer.prebuf = (uint32_t)-1;
    requestedBuffer.tlength = m_bufferSize;

    if (pa_stream_connect_playback(m_stream, m_device.data(), (m_bufferSize > 0) ? &requestedBuffer : NULL, (pa_stream_flags_t)0, NULL, NULL) < 0) {
        qWarning() << "pa_stream_connect_playback() failed!";
        pa_stream_unref(m_stream);
        m_stream = 0;
        pulseEngine->unlock();
        setError(QAudio::OpenError);
        setState(QAudio::StoppedState);
        return false;
    }

    while (pa_stream_get_state(m_stream) != PA_STREAM_READY)
        pa_threaded_mainloop_wait(pulseEngine->mainloop());

    const pa_buffer_attr *buffer = pa_stream_get_buffer_attr(m_stream);
    m_periodTime = (m_category == LOW_LATENCY_CATEGORY_NAME) ? LowLatencyPeriodTimeMs : PeriodTimeMs;
    m_periodSize = pa_usec_to_bytes(m_periodTime*1000, &m_spec);
    m_bufferSize = buffer->tlength;
    m_maxBufferSize = buffer->maxlength;
    m_audioBuffer = new char[m_maxBufferSize];

    const qint64 streamSize = m_audioSource ? m_audioSource->size() : 0;
    if (m_pullMode && streamSize > 0 && static_cast<qint64>(buffer->prebuf) > streamSize) {
        pa_buffer_attr newBufferAttr;
        newBufferAttr = *buffer;
        newBufferAttr.prebuf = streamSize;
        pa_operation *o = pa_stream_set_buffer_attr(m_stream, &newBufferAttr, streamAdjustPrebufferCallback, NULL);
        if (o)
            pa_operation_unref(o);
    }

#ifdef DEBUG_PULSE
    qDebug() << "Buffering info:";
    qDebug() << "\tMax length: " << buffer->maxlength;
    qDebug() << "\tTarget length: " << buffer->tlength;
    qDebug() << "\tPre-buffering: " << buffer->prebuf;
    qDebug() << "\tMinimum request: " << buffer->minreq;
    qDebug() << "\tFragment size: " << buffer->fragsize;
#endif

    pulseEngine->unlock();

    connect(pulseEngine, &QPulseAudioEngine::contextFailed, this, &QPulseAudioOutput::onPulseContextFailed);

    m_opened = true;

    m_tickTimer->start(m_periodTime);

    m_elapsedTimeOffset = 0;
    m_timeStamp.restart();
    m_clockStamp.restart();

    return true;
}

void QPulseAudioOutput::close()
{
    if (!m_opened)
        return;

    m_tickTimer->stop();

    QPulseAudioEngine *pulseEngine = QPulseAudioEngine::instance();

    if (m_stream) {
        pulseEngine->lock();

        pa_stream_set_state_callback(m_stream, 0, 0);
        pa_stream_set_write_callback(m_stream, 0, 0);
        pa_stream_set_underflow_callback(m_stream, 0, 0);
        pa_stream_set_overflow_callback(m_stream, 0, 0);
        pa_stream_set_latency_update_callback(m_stream, 0, 0);

        pa_operation *o = pa_stream_drain(m_stream, outputStreamDrainComplete, NULL);
        if (o)
            pa_operation_unref(o);

        pa_stream_disconnect(m_stream);
        pa_stream_unref(m_stream);
        m_stream = NULL;

        pulseEngine->unlock();
    }

    disconnect(pulseEngine, &QPulseAudioEngine::contextFailed, this, &QPulseAudioOutput::onPulseContextFailed);

    if (!m_pullMode && m_audioSource) {
        delete m_audioSource;
        m_audioSource = 0;
    }
    m_opened = false;
    if (m_audioBuffer) {
        delete[] m_audioBuffer;
        m_audioBuffer = 0;
    }
}

void QPulseAudioOutput::userFeed()
{
    if (m_deviceState == QAudio::StoppedState || m_deviceState == QAudio::SuspendedState)
        return;

    m_resuming = false;

    if (m_pullMode) {
        int writableSize = bytesFree();
        int chunks = writableSize / m_periodSize;
        if (chunks == 0)
            return;

        int input = m_periodSize; // always request 1 chunk of data from user
        if (input > m_maxBufferSize)
            input = m_maxBufferSize;

        int audioBytesPulled = m_audioSource->read(m_audioBuffer, input);
        Q_ASSERT(audioBytesPulled <= input);
        if (audioBytesPulled > 0) {
            if (audioBytesPulled > input) {
                qWarning() << "QPulseAudioOutput::userFeed() - Invalid audio data size provided from user:"
                           << audioBytesPulled << "should be less than" << input;
                audioBytesPulled = input;
            }
            qint64 bytesWritten = write(m_audioBuffer, audioBytesPulled);
            Q_ASSERT(bytesWritten == audioBytesPulled); //unfinished write should not happen since the data provided is less than writableSize
            Q_UNUSED(bytesWritten);

            if (chunks > 1) {
                // PulseAudio needs more data. Ask for it immediately.
                QMetaObject::invokeMethod(this, "userFeed", Qt::QueuedConnection);
            }
        }
    }

    if (m_deviceState != QAudio::ActiveState)
        return;

    if (m_notifyInterval && (m_timeStamp.elapsed() + m_elapsedTimeOffset) > m_notifyInterval) {
        emit notify();
        m_elapsedTimeOffset = m_timeStamp.elapsed() + m_elapsedTimeOffset - m_notifyInterval;
        m_timeStamp.restart();
    }
}

qint64 QPulseAudioOutput::write(const char *data, qint64 len)
{
    QPulseAudioEngine *pulseEngine = QPulseAudioEngine::instance();

    pulseEngine->lock();

    len = qMin(len, static_cast<qint64>(pa_stream_writable_size(m_stream)));

    if (m_volume < 1.0f) {
        // Don't use PulseAudio volume, as it might affect all other streams of the same category
        // or even affect the system volume if flat volumes are enabled
        void *dest = NULL;
        size_t nbytes = len;
        if (pa_stream_begin_write(m_stream, &dest, &nbytes) < 0) {
            qWarning("QAudioOutput(pulseaudio): pa_stream_begin_write, error = %s",
                     pa_strerror(pa_context_errno(pulseEngine->context())));
            setError(QAudio::IOError);
            return 0;
        }

        len = int(nbytes);
        QAudioHelperInternal::qMultiplySamples(m_volume, m_format, data, dest, len);
        data = reinterpret_cast<char *>(dest);
    }

    if (pa_stream_write(m_stream, data, len, NULL, 0, PA_SEEK_RELATIVE) < 0) {
        qWarning("QAudioOutput(pulseaudio): pa_stream_write, error = %s",
                 pa_strerror(pa_context_errno(pulseEngine->context())));
        setError(QAudio::IOError);
        return 0;
    }

    pulseEngine->unlock();
    m_totalTimeValue += len;

    setError(QAudio::NoError);
    setState(QAudio::ActiveState);

    return len;
}

void QPulseAudioOutput::stop()
{
    if (m_deviceState == QAudio::StoppedState)
        return;

    close();

    setError(QAudio::NoError);
    setState(QAudio::StoppedState);
}

int QPulseAudioOutput::bytesFree() const
{
    if (m_deviceState != QAudio::ActiveState && m_deviceState != QAudio::IdleState)
        return 0;

    QPulseAudioEngine *pulseEngine = QPulseAudioEngine::instance();
    pulseEngine->lock();
    int writableSize = pa_stream_writable_size(m_stream);
    pulseEngine->unlock();
    return writableSize;
}

int QPulseAudioOutput::periodSize() const
{
    return m_periodSize;
}

void QPulseAudioOutput::setBufferSize(int value)
{
    m_bufferSize = value;
}

int QPulseAudioOutput::bufferSize() const
{
    return m_bufferSize;
}

void QPulseAudioOutput::setNotifyInterval(int ms)
{
    m_notifyInterval = qMax(0, ms);
}

int QPulseAudioOutput::notifyInterval() const
{
    return m_notifyInterval;
}

qint64 QPulseAudioOutput::processedUSecs() const
{
    qint64 result = qint64(1000000) * m_totalTimeValue /
        (m_format.channelCount() * (m_format.sampleSize() / 8)) /
        m_format.sampleRate();

    return result;
}

void QPulseAudioOutput::resume()
{
    if (m_deviceState == QAudio::SuspendedState) {
        m_resuming = true;

        QPulseAudioEngine *pulseEngine = QPulseAudioEngine::instance();

        pulseEngine->lock();

        pa_operation *operation = pa_stream_cork(m_stream, 0, outputStreamSuccessCallback, NULL);
        pulseEngine->wait(operation);
        pa_operation_unref(operation);

        operation = pa_stream_trigger(m_stream, outputStreamSuccessCallback, NULL);
        pulseEngine->wait(operation);
        pa_operation_unref(operation);

        pulseEngine->unlock();

        m_tickTimer->start(m_periodTime);

        setState(m_pullMode ? QAudio::ActiveState : QAudio::IdleState);
        setError(QAudio::NoError);
    }
}

void QPulseAudioOutput::setFormat(const QAudioFormat &format)
{
    m_format = format;
}

QAudioFormat QPulseAudioOutput::format() const
{
    return m_format;
}

void QPulseAudioOutput::suspend()
{
    if (m_deviceState == QAudio::ActiveState || m_deviceState == QAudio::IdleState) {
        setError(QAudio::NoError);
        setState(QAudio::SuspendedState);

        m_tickTimer->stop();

        QPulseAudioEngine *pulseEngine = QPulseAudioEngine::instance();
        pa_operation *operation;

        pulseEngine->lock();

        operation = pa_stream_cork(m_stream, 1, outputStreamSuccessCallback, NULL);
        pulseEngine->wait(operation);
        pa_operation_unref(operation);

        pulseEngine->unlock();
    }
}

qint64 QPulseAudioOutput::elapsedUSecs() const
{
    if (m_deviceState == QAudio::StoppedState)
        return 0;

    return m_clockStamp.elapsed() * qint64(1000);
}

void QPulseAudioOutput::reset()
{
    stop();
}

PulseOutputPrivate::PulseOutputPrivate(QPulseAudioOutput *audio)
{
    m_audioDevice = qobject_cast<QPulseAudioOutput*>(audio);
}

qint64 PulseOutputPrivate::readData(char *data, qint64 len)
{
    Q_UNUSED(data)
    Q_UNUSED(len)

    return 0;
}

qint64 PulseOutputPrivate::writeData(const char *data, qint64 len)
{
    int retry = 0;
    qint64 written = 0;

    if ((m_audioDevice->m_deviceState == QAudio::ActiveState
         || m_audioDevice->m_deviceState == QAudio::IdleState)) {
         while(written < len) {
            int chunk = m_audioDevice->write(data+written, (len-written));
            if (chunk <= 0)
                retry++;
            written+=chunk;
            if (retry > 10)
                return written;
        }
    }

    return written;
}

void QPulseAudioOutput::setVolume(qreal vol)
{
    if (qFuzzyCompare(m_volume, vol))
        return;

    m_volume = qBound(qreal(0), vol, qreal(1));
}

qreal QPulseAudioOutput::volume() const
{
    return m_volume;
}

void QPulseAudioOutput::setCategory(const QString &category)
{
    if (m_category != category) {
        m_category = category;
    }
}

QString QPulseAudioOutput::category() const
{
    return m_category;
}

void QPulseAudioOutput::onPulseContextFailed()
{
    close();

    setError(QAudio::FatalError);
    setState(QAudio::StoppedState);
}

QT_END_NAMESPACE

#include "moc_qaudiooutput_pulse.cpp"
