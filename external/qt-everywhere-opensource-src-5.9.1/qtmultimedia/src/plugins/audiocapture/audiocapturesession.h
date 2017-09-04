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

#ifndef AUDIOCAPTURESESSION_H
#define AUDIOCAPTURESESSION_H

#include <QFile>
#include <QUrl>
#include <QDir>
#include <QMutex>

#include "audioencodercontrol.h"
#include "audioinputselector.h"
#include "audiomediarecordercontrol.h"

#include <qaudioformat.h>
#include <qaudioinput.h>
#include <qaudiodeviceinfo.h>

QT_BEGIN_NAMESPACE

class AudioCaptureProbeControl;

class FileProbeProxy: public QFile {
public:
    void startProbes(const QAudioFormat& format);
    void stopProbes();
    void addProbe(AudioCaptureProbeControl *probe);
    void removeProbe(AudioCaptureProbeControl *probe);

protected:
    virtual qint64 writeData(const char *data, qint64 len);

private:
    QAudioFormat m_format;
    QList<AudioCaptureProbeControl*> m_probes;
    QMutex m_probeMutex;
};


class AudioCaptureSession : public QObject
{
    Q_OBJECT

public:
    AudioCaptureSession(QObject *parent = 0);
    ~AudioCaptureSession();

    QAudioFormat format() const;
    void setFormat(const QAudioFormat &format);

    QString containerFormat() const;
    void setContainerFormat(const QString &formatMimeType);

    QUrl outputLocation() const;
    bool setOutputLocation(const QUrl& location);

    qint64 position() const;

    void setState(QMediaRecorder::State state);
    QMediaRecorder::State state() const;
    QMediaRecorder::Status status() const;

    void addProbe(AudioCaptureProbeControl *probe);
    void removeProbe(AudioCaptureProbeControl *probe);

    void setCaptureDevice(const QString &deviceName);

    void setVolume(qreal v);
    qreal volume() const;

    void setMuted(bool muted);
    bool isMuted() const;

signals:
    void stateChanged(QMediaRecorder::State state);
    void statusChanged(QMediaRecorder::Status status);
    void positionChanged(qint64 position);
    void actualLocationChanged(const QUrl &location);
    void volumeChanged(qreal volume);
    void mutedChanged(bool muted);
    void error(int error, const QString &errorString);

private slots:
    void audioInputStateChanged(QAudio::State state);
    void notify();

private:
    void record();
    void pause();
    void stop();

    void setStatus(QMediaRecorder::Status status);

    void setVolumeHelper(qreal volume);

    QDir defaultDir() const;
    QString generateFileName(const QString &requestedName,
                             const QString &extension) const;
    QString generateFileName(const QDir &dir, const QString &extension) const;

    FileProbeProxy file;
    QString m_captureDevice;
    QUrl m_requestedOutputLocation;
    QUrl m_actualOutputLocation;
    QMediaRecorder::State m_state;
    QMediaRecorder::Status m_status;
    QAudioInput *m_audioInput;
    QAudioDeviceInfo m_deviceInfo;
    QAudioFormat m_format;
    bool m_wavFile;
    qreal m_volume;
    bool m_muted;

    // WAV header stuff

    struct chunk
    {
        char        id[4];
        quint32     size;
    };

    struct RIFFHeader
    {
        chunk       descriptor;
        char        type[4];
    };

    struct WAVEHeader
    {
        chunk       descriptor;
        quint16     audioFormat;        // PCM = 1
        quint16     numChannels;
        quint32     sampleRate;
        quint32     byteRate;
        quint16     blockAlign;
        quint16     bitsPerSample;
    };

    struct DATAHeader
    {
        chunk       descriptor;
//        quint8      data[];
    };

    struct CombinedHeader
    {
        RIFFHeader  riff;
        WAVEHeader  wave;
        DATAHeader  data;
    };

    CombinedHeader      header;
};

QT_END_NAMESPACE

#endif
