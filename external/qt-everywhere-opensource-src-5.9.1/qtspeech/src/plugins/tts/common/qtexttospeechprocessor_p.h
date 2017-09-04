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

#ifndef QTEXTTOSPEECHPROCESSOR_P_H
#define QTEXTTOSPEECHPROCESSOR_P_H

#include "qvoice.h"

#include <QtCore/QString>
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QSemaphore>
#include <QtCore/QIODevice>
#include <QtMultimedia/QAudioOutput>

QT_BEGIN_NAMESPACE

// A common base class for text-to-speech engine integrations
// that require audio output implementation and thread handling.
//
// QAudioOutput is used for audio, and each call to say() cancels
// any previous processing. The public interface is thread-safe.
class QTextToSpeechProcessor : public QThread {
    Q_OBJECT

public:
    struct VoiceInfo
    {
        int id;
        QString name;
        QString locale;
        QVoice::Gender gender;
        QVoice::Age age;
    };
    QTextToSpeechProcessor();
    ~QTextToSpeechProcessor();
    void say(const QString &text, int voiceId);
    void stop();
    void pause();
    void resume();
    bool isIdle() const;
    bool setRate(double rate);
    bool setPitch(double pitch);
    bool setVolume(double volume);
    double rate() const;
    double pitch() const;
    double volume() const;
    virtual const QVector<VoiceInfo> &voices() const = 0;

protected:
    // These are re-implemented QThread methods.
    // exit() waits until the processor thread finishes or the wait times out.
    void start(QThread::Priority = QThread::InheritPriority);
    void exit(int retcode = 0);

    // These methods can be used for audio output.
    // audioOutput() blocks until all the audio has been written or processing
    // is interrupted.
    bool audioStart(int sampleRate, int channelCount, QString *errorString = 0);
    bool audioOutput(const char* data, qint64 dataSize, QString *errorString = 0);
    void audioStop(bool abort = false);

    // These methods should be re-implemented if the parameters need
    // to be changed while TTS is speaking. By default, updateVolume() just
    // changes the QAudioOutput volume. The other methods do nothing by default.
    virtual bool updateRate(double rate);
    virtual bool updatePitch(double pitch);
    virtual bool updateVolume(double volume);

    // This method is called from the internal processor thread, and should block
    // until the given text has been processed or processing is interrupted.
    virtual int processText(const QString &text, int voiceId) = 0;

signals:
    // This signal is emitted when the processor goes to idle state, i.e. when no
    // new text is set to be spoken. The parameter is the latest return value of
    // processText(). As the signal is emitted from the internal thread, the recipient
    // should call isIdle() to get updated state.
    void notSpeaking(int statusCode);

private:
    void run() override;
    mutable QMutex m_lock;
    volatile bool m_stop;
    volatile bool m_idle;
    volatile bool m_paused;
    double m_rate;
    double m_pitch;
    int m_volume;
    QSemaphore m_speakSem;
    QString m_nextText;
    int m_nextVoice;
    QAudioOutput *m_audio;
    QIODevice *m_audioBuffer;
};

QT_END_NAMESPACE

#endif
