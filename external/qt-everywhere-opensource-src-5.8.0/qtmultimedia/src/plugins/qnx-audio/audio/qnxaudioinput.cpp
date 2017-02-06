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

#include "qnxaudioinput.h"

#include "qnxaudioutils.h"

#include <private/qaudiohelpers_p.h>

#include <QDebug>

QT_BEGIN_NAMESPACE

QnxAudioInput::QnxAudioInput()
    : m_audioSource(0)
    , m_pcmHandle(0)
    , m_pcmNotifier(0)
    , m_error(QAudio::NoError)
    , m_state(QAudio::StoppedState)
    , m_bytesRead(0)
    , m_elapsedTimeOffset(0)
    , m_totalTimeValue(0)
    , m_volume(qreal(1.0f))
    , m_bytesAvailable(0)
    , m_bufferSize(0)
    , m_periodSize(0)
    , m_intervalTime(1000)
    , m_pullMode(true)
{
}

QnxAudioInput::~QnxAudioInput()
{
    close();
}

void QnxAudioInput::start(QIODevice *device)
{
    if (m_state != QAudio::StoppedState)
        close();

    if (!m_pullMode && m_audioSource)
        delete m_audioSource;

    m_pullMode = true;
    m_audioSource = device;

    if (open()) {
        setError(QAudio::NoError);
        setState(QAudio::ActiveState);
    } else {
        setError(QAudio::OpenError);
        setState(QAudio::StoppedState);
    }
}

QIODevice *QnxAudioInput::start()
{
    if (m_state != QAudio::StoppedState)
        close();

    if (!m_pullMode && m_audioSource)
        delete m_audioSource;

    m_pullMode = false;
    m_audioSource = new InputPrivate(this);
    m_audioSource->open(QIODevice::ReadOnly | QIODevice::Unbuffered);

    if (open()) {
        setError(QAudio::NoError);
        setState(QAudio::IdleState);
    } else {
        delete m_audioSource;
        m_audioSource = 0;

        setError(QAudio::OpenError);
        setState(QAudio::StoppedState);
    }

    return m_audioSource;
}

void QnxAudioInput::stop()
{
    if (m_state == QAudio::StoppedState)
        return;

    setError(QAudio::NoError);
    setState(QAudio::StoppedState);
    close();
}

void QnxAudioInput::reset()
{
    stop();
    m_bytesAvailable = 0;
}

void QnxAudioInput::suspend()
{
    if (m_pcmNotifier)
        m_pcmNotifier->setEnabled(false);

    setState(QAudio::SuspendedState);
}

void QnxAudioInput::resume()
{
    if (m_pcmNotifier)
        m_pcmNotifier->setEnabled(true);

    if (m_pullMode) {
        setState(QAudio::ActiveState);
    } else {
        setState(QAudio::IdleState);
    }
}

int QnxAudioInput::bytesReady() const
{
    return qMax(m_bytesAvailable, 0);
}

int QnxAudioInput::periodSize() const
{
    return m_periodSize;
}

void QnxAudioInput::setBufferSize(int bufferSize)
{
    m_bufferSize = bufferSize;
}

int QnxAudioInput::bufferSize() const
{
    return m_bufferSize;
}

void QnxAudioInput::setNotifyInterval(int milliSeconds)
{
    m_intervalTime = qMax(0, milliSeconds);
}

int QnxAudioInput::notifyInterval() const
{
    return m_intervalTime;
}

qint64 QnxAudioInput::processedUSecs() const
{
    return qint64(1000000) * m_format.framesForBytes(m_bytesRead) / m_format.sampleRate();
}

qint64 QnxAudioInput::elapsedUSecs() const
{
    if (m_state == QAudio::StoppedState)
        return 0;

    return m_clockStamp.elapsed() * qint64(1000);
}

QAudio::Error QnxAudioInput::error() const
{
    return m_error;
}

QAudio::State QnxAudioInput::state() const
{
    return m_state;
}

void QnxAudioInput::setFormat(const QAudioFormat &format)
{
    if (m_state == QAudio::StoppedState)
        m_format = format;
}

QAudioFormat QnxAudioInput::format() const
{
    return m_format;
}

void QnxAudioInput::setVolume(qreal volume)
{
    m_volume = qBound(qreal(0.0), volume, qreal(1.0));
}

qreal QnxAudioInput::volume() const
{
    return m_volume;
}

void QnxAudioInput::userFeed()
{
    if (m_state == QAudio::StoppedState || m_state == QAudio::SuspendedState)
        return;

    deviceReady();
}

bool QnxAudioInput::deviceReady()
{
    if (m_pullMode) {
        // reads some audio data and writes it to QIODevice
        read(0, 0);
    } else {
        m_bytesAvailable = m_periodSize;

        // emits readyRead() so user will call read() on QIODevice to get some audio data
        if (m_audioSource != 0) {
            InputPrivate *input = qobject_cast<InputPrivate*>(m_audioSource);
            input->trigger();
        }
    }

    if (m_state != QAudio::ActiveState)
        return true;

    if (m_intervalTime && (m_timeStamp.elapsed() + m_elapsedTimeOffset) > m_intervalTime) {
        emit notify();
        m_elapsedTimeOffset = m_timeStamp.elapsed() + m_elapsedTimeOffset - m_intervalTime;
        m_timeStamp.restart();
    }

    return true;
}

bool QnxAudioInput::open()
{
    if (!m_format.isValid() || m_format.sampleRate() <= 0) {
        if (!m_format.isValid())
            qWarning("QnxAudioInput: open error, invalid format.");
        else
            qWarning("QnxAudioInput: open error, invalid sample rate (%d).", m_format.sampleRate());

        return false;
    }

    int errorCode = 0;

    int card = 0;
    int device = 0;
    if ((errorCode = snd_pcm_open_preferred(&m_pcmHandle, &card, &device, SND_PCM_OPEN_CAPTURE)) < 0) {
        qWarning("QnxAudioInput: open error, couldn't open card (0x%x)", -errorCode);
        return false;
    }

    // Necessary so that bytesFree() which uses the "free" member of the status struct works
    snd_pcm_plugin_set_disable(m_pcmHandle, PLUGIN_MMAP);

    snd_pcm_channel_info_t info;
    memset(&info, 0, sizeof(info));
    info.channel = SND_PCM_CHANNEL_CAPTURE;
    if ((errorCode = snd_pcm_plugin_info(m_pcmHandle, &info)) < 0) {
        qWarning("QnxAudioInput: open error, couldn't get channel info (0x%x)", -errorCode);
        close();
        return false;
    }

    snd_pcm_channel_params_t params = QnxAudioUtils::formatToChannelParams(m_format, QAudio::AudioInput, info.max_fragment_size);

    if ((errorCode = snd_pcm_plugin_params(m_pcmHandle, &params)) < 0) {
        qWarning("QnxAudioInput: open error, couldn't set channel params (0x%x)", -errorCode);
        close();
        return false;
    }

    if ((errorCode = snd_pcm_plugin_prepare(m_pcmHandle, SND_PCM_CHANNEL_CAPTURE)) < 0) {
        qWarning("QnxAudioInput: open error, couldn't prepare channel (0x%x)", -errorCode);
        close();
        return false;
    }

    snd_pcm_channel_setup_t setup;

    memset(&setup, 0, sizeof(setup));
    setup.channel = SND_PCM_CHANNEL_CAPTURE;
    if ((errorCode = snd_pcm_plugin_setup(m_pcmHandle, &setup)) < 0) {
        qWarning("QnxAudioInput: open error, couldn't get channel setup (0x%x)", -errorCode);
        close();
        return false;
    }

    m_periodSize = qMin(2048, setup.buf.block.frag_size);

    m_clockStamp.restart();
    m_timeStamp.restart();
    m_elapsedTimeOffset = 0;
    m_totalTimeValue = 0;
    m_bytesRead = 0;

    m_pcmNotifier = new QSocketNotifier(snd_pcm_file_descriptor(m_pcmHandle, SND_PCM_CHANNEL_CAPTURE),
                                        QSocketNotifier::Read, this);
    connect(m_pcmNotifier, SIGNAL(activated(int)), SLOT(userFeed()));

    return true;
}

void QnxAudioInput::close()
{
    if (m_pcmHandle)
        snd_pcm_plugin_flush(m_pcmHandle, SND_PCM_CHANNEL_CAPTURE);

    if (m_pcmNotifier) {
        delete m_pcmNotifier;
        m_pcmNotifier = 0;
    }

    if (m_pcmHandle) {
        snd_pcm_close(m_pcmHandle);
        m_pcmHandle = 0;
    }

    if (!m_pullMode && m_audioSource) {
        delete m_audioSource;
        m_audioSource = 0;
    }
}

qint64 QnxAudioInput::read(char *data, qint64 len)
{
    int errorCode = 0;
    QByteArray tempBuffer(m_periodSize, 0);

    const int actualRead = snd_pcm_plugin_read(m_pcmHandle, tempBuffer.data(), m_periodSize);
    if (actualRead < 1) {
        snd_pcm_channel_status_t status;
        memset(&status, 0, sizeof(status));
        status.channel = SND_PCM_CHANNEL_CAPTURE;
        if ((errorCode = snd_pcm_plugin_status(m_pcmHandle, &status)) < 0) {
            qWarning("QnxAudioInput: read error, couldn't get plugin status (0x%x)", -errorCode);
            close();
            setError(QAudio::FatalError);
            setState(QAudio::StoppedState);
            return -1;
        }

        if (status.status == SND_PCM_STATUS_READY
            || status.status == SND_PCM_STATUS_OVERRUN) {
            if ((errorCode = snd_pcm_plugin_prepare(m_pcmHandle, SND_PCM_CHANNEL_CAPTURE)) < 0) {
                qWarning("QnxAudioInput: read error, couldn't prepare plugin (0x%x)", -errorCode);
                close();
                setError(QAudio::FatalError);
                setState(QAudio::StoppedState);
                return -1;
            }
        }
    } else {
        setError(QAudio::NoError);
        setState(QAudio::ActiveState);
    }

    if (m_volume < 1.0f)
        QAudioHelperInternal::qMultiplySamples(m_volume, m_format, tempBuffer.data(), tempBuffer.data(), actualRead);

    m_bytesRead += actualRead;

    if (m_pullMode) {
        m_audioSource->write(tempBuffer.data(), actualRead);
    } else {
        memcpy(data, tempBuffer.data(), qMin(static_cast<qint64>(actualRead), len));
    }

    m_bytesAvailable = 0;

    return actualRead;
}

void QnxAudioInput::setError(QAudio::Error error)
{
    if (m_error == error)
        return;

    m_error = error;
    emit errorChanged(m_error);
}

void QnxAudioInput::setState(QAudio::State state)
{
    if (m_state == state)
        return;

    m_state = state;
    emit stateChanged(m_state);
}

InputPrivate::InputPrivate(QnxAudioInput *audio)
    : m_audioDevice(audio)
{
}

qint64 InputPrivate::readData(char *data, qint64 len)
{
    return m_audioDevice->read(data, len);
}

qint64 InputPrivate::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data)
    Q_UNUSED(len)
    return 0;
}

void InputPrivate::trigger()
{
    emit readyRead();
}

QT_END_NAMESPACE
