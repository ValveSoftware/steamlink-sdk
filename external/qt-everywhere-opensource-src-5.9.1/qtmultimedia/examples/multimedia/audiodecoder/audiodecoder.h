/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
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
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
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

#ifndef AUDIODECODER_H
#define AUDIODECODER_H

#include "wavefilewriter.h"

#include <QAudioDecoder>
#include <QSoundEffect>
#include <QTextStream>

class AudioDecoder : public QObject
{
    Q_OBJECT

public:
    AudioDecoder(bool isPlayback, bool isDelete);
    ~AudioDecoder() { }

    void setSourceFilename(const QString &fileName);
    void start();
    void stop();

    void setTargetFilename(const QString &fileName);

signals:
    void done();

public slots:
    void bufferReady();
    void error(QAudioDecoder::Error error);
    void stateChanged(QAudioDecoder::State newState);
    void finished();

    void playbackStatusChanged();
    void playingChanged();

private slots:
    void updateProgress();

private:
    bool m_isPlayback;
    bool m_isDelete;
    QAudioDecoder m_decoder;
    QTextStream m_cout;

    QString m_targetFilename;
    WaveFileWriter m_fileWriter;
    QSoundEffect m_soundEffect;

    qreal m_progress;
};

#endif // AUDIODECODER_H
