/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
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

#include "qnxaudiooutput.h"

#include "qnxaudioutils.h"

#include <private/qaudiohelpers_p.h>

QT_BEGIN_NAMESPACE

QnxAudioOutput::QnxAudioOutput()
    : m_source(0),
      m_pushSource(false),
      m_notifyInterval(1000),
      m_error(QAudio::NoError),
      m_state(QAudio::StoppedState),
      m_volume(1.0),
      m_periodSize(0),
      m_pcmHandle(0),
      m_bytesWritten(0),
      m_intervalOffset(0)
{
    m_timer.setSingleShot(false);
    m_timer.setInterval(20);
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(pullData()));
}

QnxAudioOutput::~QnxAudioOutput()
{
    stop();
}

void QnxAudioOutput::start(QIODevice *source)
{
    if (m_state != QAudio::StoppedState)
        stop();

    m_error = QAudio::NoError;
    m_source = source;
    m_pushSource = false;

    if (open()) {
        setState(QAudio::ActiveState);
        m_timer.start();
    } else {
        setError(QAudio::OpenError);
        setState(QAudio::StoppedState);
    }
}

QIODevice *QnxAudioOutput::start()
{
    if (m_state != QAudio::StoppedState)
        stop();

    m_error = QAudio::NoError;
    m_source = new QnxPushIODevice(this);
    m_source->open(QIODevice::WriteOnly|QIODevice::Unbuffered);
    m_pushSource = true;

    if (open())
        setState(QAudio::IdleState);
    else {
        setError(QAudio::OpenError);
        setState(QAudio::StoppedState);
    }

    return m_source;
}

void QnxAudioOutput::stop()
{
    if (m_state == QAudio::StoppedState)
        return;

    setError(QAudio::NoError);
    setState(QAudio::StoppedState);
    close();
}

void QnxAudioOutput::reset()
{
    if (m_pcmHandle)
        snd_pcm_playback_drain(m_pcmHandle);
    stop();
}

void QnxAudioOutput::suspend()
{
    m_timer.stop();
    setState(QAudio::SuspendedState);
}

void QnxAudioOutput::resume()
{
    if (m_pushSource)
        setState(QAudio::IdleState);
    else {
        setState(QAudio::ActiveState);
        m_timer.start();
    }
}

int QnxAudioOutput::bytesFree() const
{
    if (m_state != QAudio::ActiveState && m_state != QAudio::IdleState)
        return 0;

    snd_pcm_channel_status_t status;
    status.channel = SND_PCM_CHANNEL_PLAYBACK;
    const int errorCode = snd_pcm_plugin_status(m_pcmHandle, &status);

    if (errorCode)
        return 0;
    else
        return status.free;
}

int QnxAudioOutput::periodSize() const
{
     return m_periodSize;
}

void QnxAudioOutput::setNotifyInterval(int ms)
{
    m_notifyInterval = ms;
}

int QnxAudioOutput::notifyInterval() const
{
    return m_notifyInterval;
}

qint64 QnxAudioOutput::processedUSecs() const
{
    return qint64(1000000) * m_format.framesForBytes(m_bytesWritten) / m_format.sampleRate();
}

qint64 QnxAudioOutput::elapsedUSecs() const
{
    if (m_state == QAudio::StoppedState)
        return 0;
    else
        return m_startTimeStamp.elapsed() * qint64(1000);
}

QAudio::Error QnxAudioOutput::error() const
{
    return m_error;
}

QAudio::State QnxAudioOutput::state() const
{
    return m_state;
}

void QnxAudioOutput::setFormat(const QAudioFormat &format)
{
    if (m_state == QAudio::StoppedState)
        m_format = format;
}

QAudioFormat QnxAudioOutput::format() const
{
    return m_format;
}

void QnxAudioOutput::setVolume(qreal volume)
{
    m_volume = qBound(qreal(0.0), volume, qreal(1.0));
}

qreal QnxAudioOutput::volume() const
{
    return m_volume;
}

void QnxAudioOutput::pullData()
{
    if (m_state == QAudio::StoppedState || m_state == QAudio::SuspendedState)
        return;

    const int bytesAvailable = bytesFree();
    const int frames = m_format.framesForBytes(bytesAvailable);

    if (frames == 0 || bytesAvailable < periodSize())
        return;

    const int bytesRequested = m_format.bytesForFrames(frames);

    char buffer[bytesRequested];
    const int bytesRead = m_source->read(buffer, bytesRequested);

    // reading can take a while and stream may have been stopped
    if (!m_pcmHandle)
        return;

    if (bytesRead > 0) {
        // Got some data to output
        if (m_state != QAudio::ActiveState)
            return;

        const qint64 bytesWritten = write(buffer, bytesRead);
        if (bytesWritten != bytesRead)
            m_source->seek(m_source->pos()-(bytesRead-bytesWritten));

    } else {
        // We're done
        close();
        if (bytesRead != 0)
            setError(QAudio::IOError);
        setState(QAudio::StoppedState);
    }

    if (m_state != QAudio::ActiveState)
        return;

    if (m_notifyInterval > 0 && (m_intervalTimeStamp.elapsed() + m_intervalOffset) > m_notifyInterval) {
        emit notify();
        m_intervalOffset = m_intervalTimeStamp.elapsed() + m_intervalOffset - m_notifyInterval;
        m_intervalTimeStamp.restart();
    }
}

bool QnxAudioOutput::open()
{
    if (!m_format.isValid() || m_format.sampleRate() <= 0) {
        if (!m_format.isValid())
            qWarning("QnxAudioOutput: open error, invalid format.");
        else
            qWarning("QnxAudioOutput: open error, invalid sample rate (%d).", m_format.sampleRate());

        return false;
    }

    int errorCode = 0;

    int card = 0;
    int device = 0;
    if ((errorCode = snd_pcm_open_preferred(&m_pcmHandle, &card, &device, SND_PCM_OPEN_PLAYBACK)) < 0) {
        qWarning("QnxAudioOutput: open error, couldn't open card (0x%x)", -errorCode);
        return false;
    }

    if ((errorCode = snd_pcm_nonblock_mode(m_pcmHandle, 0)) < 0) {
        qWarning("QnxAudioOutput: open error, couldn't set non block mode (0x%x)", -errorCode);
        close();
        return false;
    }

    // Necessary so that bytesFree() which uses the "free" member of the status struct works
    snd_pcm_plugin_set_disable(m_pcmHandle, PLUGIN_MMAP);

    snd_pcm_channel_info_t info;
    memset(&info, 0, sizeof(info));
    info.channel = SND_PCM_CHANNEL_PLAYBACK;
    if ((errorCode = snd_pcm_plugin_info(m_pcmHandle, &info)) < 0) {
        qWarning("QnxAudioOutput: open error, couldn't get channel info (0x%x)", -errorCode);
        close();
        return false;
    }

    snd_pcm_channel_params_t params = QnxAudioUtils::formatToChannelParams(m_format, QAudio::AudioOutput, info.max_fragment_size);

    if ((errorCode = snd_pcm_plugin_params(m_pcmHandle, &params)) < 0) {
        qWarning("QnxAudioOutput: open error, couldn't set channel params (0x%x)", -errorCode);
        close();
        return false;
    }

    if ((errorCode = snd_pcm_plugin_prepare(m_pcmHandle, SND_PCM_CHANNEL_PLAYBACK)) < 0) {
        qWarning("QnxAudioOutput: open error, couldn't prepare channel (0x%x)", -errorCode);
        close();
        return false;
    }

    snd_pcm_channel_setup_t setup;
    memset(&setup, 0, sizeof(setup));
    setup.channel = SND_PCM_CHANNEL_PLAYBACK;
    if ((errorCode = snd_pcm_plugin_setup(m_pcmHandle, &setup)) < 0) {
        qWarning("QnxAudioOutput: open error, couldn't get channel setup (0x%x)", -errorCode);
        close();
        return false;
    }

    m_periodSize = qMin(2048, setup.buf.block.frag_size);
    m_startTimeStamp.restart();
    m_intervalTimeStamp.restart();
    m_intervalOffset = 0;
    m_bytesWritten = 0;

    return true;
}

void QnxAudioOutput::close()
{
    m_timer.stop();

    if (m_pcmHandle) {
        snd_pcm_plugin_flush(m_pcmHandle, SND_PCM_CHANNEL_PLAYBACK);
        snd_pcm_close(m_pcmHandle);
        m_pcmHandle = 0;
    }

    if (m_pushSource) {
        delete m_source;
        m_source = 0;
    }
}

void QnxAudioOutput::setError(QAudio::Error error)
{
    if (m_error != error) {
        m_error = error;
        emit errorChanged(error);
    }
}

void QnxAudioOutput::setState(QAudio::State state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }
}

qint64 QnxAudioOutput::write(const char *data, qint64 len)
{
    if (!m_pcmHandle)
        return 0;

    // Make sure we're writing (N * frame) worth of bytes
    const int size = m_format.bytesForFrames(qBound(qint64(0), qint64(bytesFree()), len) / m_format.bytesPerFrame());

    if (size == 0)
        return 0;

    int written = 0;

    if (m_volume < 1.0f) {
        char out[size];
        QAudioHelperInternal::qMultiplySamples(m_volume, m_format, data, out, size);
        written = snd_pcm_plugin_write(m_pcmHandle, out, size);
    } else {
        written = snd_pcm_plugin_write(m_pcmHandle, data, size);
    }

    if (written > 0) {
        m_bytesWritten += written;
        setError(QAudio::NoError);
        setState(QAudio::ActiveState);
        return written;
    } else {
        close();
        setError(QAudio::FatalError);
        setState(QAudio::StoppedState);
        return 0;
    }
}

QnxPushIODevice::QnxPushIODevice(QnxAudioOutput *output)
    : QIODevice(output),
      m_output(output)
{
}

QnxPushIODevice::~QnxPushIODevice()
{
}

qint64 QnxPushIODevice::readData(char *data, qint64 len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);
    return 0;
}

qint64 QnxPushIODevice::writeData(const char *data, qint64 len)
{
    int retry = 0;
    qint64 written = 0;

    if (m_output->state() == QAudio::ActiveState
     || m_output->state() == QAudio::IdleState) {
        while (written < len) {
            const int writeSize = m_output->write(data + written, len - written);

            if (writeSize <= 0) {
                retry++;
                if (retry > 10)
                    return written;
                else
                    continue;
            }

            retry = 0;
            written += writeSize;
        }
    }

    return written;
}

QT_END_NAMESPACE
