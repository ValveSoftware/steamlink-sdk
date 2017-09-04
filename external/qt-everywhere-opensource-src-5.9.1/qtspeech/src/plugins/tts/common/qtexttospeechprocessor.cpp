/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Speech module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qtexttospeechprocessor_p.h"

#include <QtCore/QDateTime>

QT_BEGIN_NAMESPACE

QTextToSpeechProcessor::QTextToSpeechProcessor():
    m_stop(true),
    m_idle(true),
    m_paused(false),
    m_rate(0),
    m_pitch(0),
    m_volume(100),
    m_audio(0),
    m_audioBuffer(0)
{
}

QTextToSpeechProcessor::~QTextToSpeechProcessor()
{
}

void QTextToSpeechProcessor::say(const QString &text, int voiceId)
{
    if (isInterruptionRequested())
        return;
    QMutexLocker lock(&m_lock);
    bool wasPaused = m_paused;
    m_stop = true; // Cancel any previous utterance
    m_idle = false;
    m_paused = false;
    m_nextText = text;
    m_nextVoice = voiceId;
    // If the speech was paused, one signal is needed to release the pause
    // and another to start processing the new text.
    m_speakSem.release(wasPaused ? 2 : 1);
}

void QTextToSpeechProcessor::stop()
{
    QMutexLocker lock(&m_lock);
    m_stop = true;
    m_paused = false;
    m_nextText.clear();
    m_speakSem.release();
}

void QTextToSpeechProcessor::pause()
{
    QMutexLocker lock(&m_lock);
    m_paused = true;
    m_speakSem.release();
}

void QTextToSpeechProcessor::resume()
{
    QMutexLocker lock(&m_lock);
    m_paused = false;
    m_speakSem.release();
}

bool QTextToSpeechProcessor::setRate(double rate)
{
    QMutexLocker lock(&m_lock);
    if (rate >= -1.0 && rate <= 1.0) {
        if (updateRate(rate)) {
            m_rate = rate;
            return true;
        }
    }
    return false;
}

bool QTextToSpeechProcessor::setPitch(double pitch)
{
    QMutexLocker lock(&m_lock);
    if (pitch >= -1.0 && pitch <= 1.0) {
        if (updatePitch(pitch)) {
            m_pitch = pitch;
            return true;
        }
    }
    return false;
}

bool QTextToSpeechProcessor::setVolume(double volume)
{
    QMutexLocker lock(&m_lock);
    if (volume >= 0.0 && volume <= 1.0) {
        if (updateVolume(volume)) {
            m_volume = volume;
            return true;
        }
    }
    return false;
}

bool QTextToSpeechProcessor::isIdle() const
{
    QMutexLocker lock(&m_lock);
    return m_idle;
}

double QTextToSpeechProcessor::rate() const
{
    QMutexLocker lock(&m_lock);
    return m_rate;
}

double QTextToSpeechProcessor::pitch() const
{
    QMutexLocker lock(&m_lock);
    return m_pitch;
}

double QTextToSpeechProcessor::volume() const
{
    QMutexLocker lock(&m_lock);
    return m_volume;
}

void QTextToSpeechProcessor::start(QThread::Priority priority)
{
    QThread::start(priority);
}

void QTextToSpeechProcessor::exit(int retcode)
{
    QThread::exit(retcode);
    QThread::requestInterruption();
    stop();
    if (!QThread::wait(5000)) {
        QThread::terminate();
        QThread::wait();
    }
}

void QTextToSpeechProcessor::run()
{
    int statusCode = 0;
    forever {
        m_lock.lock();
        if (!m_speakSem.tryAcquire()) {
            m_idle = true;
            m_lock.unlock();
            emit notSpeaking(statusCode); // Going idle
            m_speakSem.acquire();
            m_lock.lock();
        }
        if (isInterruptionRequested()) {
            if (m_audio) {
                delete m_audio;
                m_audio = 0;
                m_audioBuffer = 0;
            }
            m_lock.unlock();
            break;
        }
        m_stop = false;
        if (!m_nextText.isEmpty()) {
            QString text = m_nextText;
            int voice = m_nextVoice;
            m_nextText.clear();
            m_lock.unlock();
            statusCode = processText(text, voice);
        } else {
            m_lock.unlock();
        }
    }
}

bool QTextToSpeechProcessor::audioStart(int sampleRate, int channelCount, QString *errorString)
{
    QMutexLocker lock(&m_lock);
    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(channelCount);
    format.setSampleSize(16);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setCodec("audio/pcm");
    if (errorString)
        *errorString = QString();
    if (m_audio)
        delete m_audio;
    m_audio = new QAudioOutput(format);
    m_audioBuffer = m_audio->start();
    updateVolume(m_volume);
    if (m_audioBuffer && m_audio->state() == QAudio::IdleState)
        return true;
    if (errorString)
        *errorString = QLatin1String("Failed to start audio output (error ")
            + QString::number(m_audio->error()) + QLatin1Char(')');
    delete m_audio;
    m_audio = 0;
    m_audioBuffer = 0;
    return false;
}

bool QTextToSpeechProcessor::audioOutput(const char *data, qint64 dataSize, QString *errorString)
{
    bool ret = true;
    int bytesWritten = 0;
    QString error;
    forever {
        m_lock.lock();
        if (m_paused) {
            m_audio->suspend();
            do {
                m_lock.unlock();
                m_speakSem.acquire(); // Wait for any command
                m_lock.lock();
            } while (m_paused);
            m_audio->resume();
        }
        if (m_stop || !m_audioBuffer
        || m_audio->state() == QAudio::StoppedState || isInterruptionRequested()) {
            if (m_audio->error() != QAudio::NoError) {
                error = QLatin1String("Audio error (")
                        + QString::number(m_audio->error()) + QLatin1Char(')');
            }
            m_lock.unlock();
            ret = false;
            break;
        }
        bytesWritten += m_audioBuffer->write(data + bytesWritten, dataSize - bytesWritten);
        m_lock.unlock();
        if (bytesWritten >= dataSize)
            break;
        QThread::msleep(50);
    }
    if (errorString)
        *errorString = error;
    return ret;
}

void QTextToSpeechProcessor::audioStop(bool abort)
{
    QMutexLocker lock(&m_lock);
    if (m_audio) {
        if (abort) {
            m_audio->reset(); // Discard buffered audio
        } else {
            // TODO: Find a way to reliably check if all the audio has been written out before stopping
            m_audioBuffer->write(QByteArray(1024, 0));
            QThread::msleep(200);
            m_audio->stop();
        }
        delete m_audio;
        m_audio = 0;
        m_audioBuffer = 0;
    }
}

bool QTextToSpeechProcessor::updateRate(double rate)
{
    Q_UNUSED(rate);
    return true;
}

bool QTextToSpeechProcessor::updatePitch(double pitch)
{
    Q_UNUSED(pitch);
    return true;
}

bool QTextToSpeechProcessor::updateVolume(double volume)
{
    if (m_audio)
        m_audio->setVolume(volume);
    return true;
}


QT_END_NAMESPACE
