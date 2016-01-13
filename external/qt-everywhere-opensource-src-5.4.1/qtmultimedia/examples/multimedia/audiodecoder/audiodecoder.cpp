/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "audiodecoder.h"
#include <stdio.h>

AudioDecoder::AudioDecoder(bool isPlayback, bool isDelete)
    : m_cout(stdout, QIODevice::WriteOnly)
{
    m_isPlayback = isPlayback;
    m_isDelete = isDelete;

    // Make sure the data we receive is in correct PCM format.
    // Our wav file writer only supports SignedInt sample type.
    QAudioFormat format;
    format.setChannelCount(2);
    format.setSampleSize(16);
    format.setSampleRate(48000);
    format.setCodec("audio/pcm");
    format.setSampleType(QAudioFormat::SignedInt);
    m_decoder.setAudioFormat(format);

    connect(&m_decoder, SIGNAL(bufferReady()), this, SLOT(bufferReady()));
    connect(&m_decoder, SIGNAL(error(QAudioDecoder::Error)), this, SLOT(error(QAudioDecoder::Error)));
    connect(&m_decoder, SIGNAL(stateChanged(QAudioDecoder::State)), this, SLOT(stateChanged(QAudioDecoder::State)));
    connect(&m_decoder, SIGNAL(finished()), this, SLOT(finished()));
    connect(&m_decoder, SIGNAL(positionChanged(qint64)), this, SLOT(updateProgress()));
    connect(&m_decoder, SIGNAL(durationChanged(qint64)), this, SLOT(updateProgress()));

    connect(&m_soundEffect, SIGNAL(statusChanged()), this, SLOT(playbackStatusChanged()));
    connect(&m_soundEffect, SIGNAL(playingChanged()), this, SLOT(playingChanged()));

    m_progress = -1.0;
}

void AudioDecoder::setSourceFilename(const QString &fileName)
{
    m_decoder.setSourceFilename(fileName);
}

void AudioDecoder::start()
{
    m_decoder.start();
}

void AudioDecoder::stop()
{
    m_decoder.stop();
}

void AudioDecoder::setTargetFilename(const QString &fileName)
{
    m_targetFilename = fileName;
}

void AudioDecoder::bufferReady()
{
    // read a buffer from audio decoder
    QAudioBuffer buffer = m_decoder.read();
    if (!buffer.isValid())
        return;

    if (!m_fileWriter.isOpen() && !m_fileWriter.open(m_targetFilename, buffer.format())) {
        m_decoder.stop();
        return;
    }

    m_fileWriter.write(buffer);
}

void AudioDecoder::error(QAudioDecoder::Error error)
{
    switch (error) {
    case QAudioDecoder::NoError:
        return;
    case QAudioDecoder::ResourceError:
        m_cout << "Resource error" << endl;
        break;
    case QAudioDecoder::FormatError:
        m_cout << "Format error" << endl;
        break;
    case QAudioDecoder::AccessDeniedError:
        m_cout << "Access denied error" << endl;
        break;
    case QAudioDecoder::ServiceMissingError:
        m_cout << "Service missing error" << endl;
        break;
    }

    emit done();
}

void AudioDecoder::stateChanged(QAudioDecoder::State newState)
{
    switch (newState) {
    case QAudioDecoder::DecodingState:
        m_cout << "Decoding..." << endl;
        break;
    case QAudioDecoder::StoppedState:
        m_cout << "Decoding stopped" << endl;
        break;
    }
}

void AudioDecoder::finished()
{
    if (!m_fileWriter.close())
        m_cout << "Failed to finilize output file" << endl;

    m_cout << "Decoding finished" << endl;

    if (m_isPlayback) {
        m_cout << "Starting playback" << endl;
        m_soundEffect.setSource(QUrl::fromLocalFile(m_targetFilename));
        m_soundEffect.play();
    } else {
        emit done();
    }
}

void AudioDecoder::playbackStatusChanged()
{
    if (m_soundEffect.status() == QSoundEffect::Error) {
        m_cout << "Playback error" << endl;
        emit done();
    }
}

void AudioDecoder::playingChanged()
{
    if (!m_soundEffect.isPlaying()) {
        m_cout << "Playback finished" << endl;
        if (m_isDelete)
            QFile::remove(m_targetFilename);
        emit done();
    }
}

void AudioDecoder::updateProgress()
{
    qint64 position = m_decoder.position();
    qint64 duration = m_decoder.duration();
    qreal progress = m_progress;
    if (position >= 0 && duration > 0)
        progress = position / (qreal)duration;

    if (progress > m_progress + 0.1) {
        m_cout << "Decoding progress: " << (int)(progress * 100.0) << "%" << endl;
        m_progress = progress;
    }
}
