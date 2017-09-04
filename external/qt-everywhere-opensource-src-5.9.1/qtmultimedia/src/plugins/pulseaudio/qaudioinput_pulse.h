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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#ifndef QAUDIOINPUTPULSE_H
#define QAUDIOINPUTPULSE_H

#include <QtCore/qfile.h>
#include <QtCore/qtimer.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qiodevice.h>

#include "qaudio.h"
#include "qaudiodeviceinfo.h"
#include "qaudiosystem.h"

#include <pulse/pulseaudio.h>

QT_BEGIN_NAMESPACE

class PulseInputPrivate;

class QPulseAudioInput : public QAbstractAudioInput
{
    Q_OBJECT

public:
    QPulseAudioInput(const QByteArray &device);
    ~QPulseAudioInput();

    qint64 read(char *data, qint64 len);

    void start(QIODevice *device);
    QIODevice *start();
    void stop();
    void reset();
    void suspend();
    void resume();
    int bytesReady() const;
    int periodSize() const;
    void setBufferSize(int value);
    int bufferSize() const;
    void setNotifyInterval(int milliSeconds);
    int notifyInterval() const;
    qint64 processedUSecs() const;
    qint64 elapsedUSecs() const;
    QAudio::Error error() const;
    QAudio::State state() const;
    void setFormat(const QAudioFormat &format);
    QAudioFormat format() const;

    void setVolume(qreal volume);
    qreal volume() const;

    qint64 m_totalTimeValue;
    QIODevice *m_audioSource;
    QAudioFormat m_format;
    QAudio::Error m_errorState;
    QAudio::State m_deviceState;
    qreal m_volume;

private slots:
    void userFeed();
    bool deviceReady();
    void onPulseContextFailed();

private:
    void setState(QAudio::State state);
    void setError(QAudio::Error error);

    void applyVolume(const void *src, void *dest, int len);

    int checkBytesReady();
    bool open();
    void close();

    bool m_pullMode;
    bool m_opened;
    int m_bytesAvailable;
    int m_bufferSize;
    int m_periodSize;
    int m_intervalTime;
    unsigned int m_periodTime;
    QTimer *m_timer;
    qint64 m_elapsedTimeOffset;
    pa_stream *m_stream;
    QTime m_timeStamp;
    QTime m_clockStamp;
    QByteArray m_streamName;
    QByteArray m_device;
    QByteArray m_tempBuffer;
    pa_sample_spec m_spec;
};

class PulseInputPrivate : public QIODevice
{
    Q_OBJECT
public:
    PulseInputPrivate(QPulseAudioInput *audio);
    ~PulseInputPrivate() {};

    qint64 readData(char *data, qint64 len);
    qint64 writeData(const char *data, qint64 len);

    void trigger();

private:
    QPulseAudioInput *m_audioDevice;
};

QT_END_NAMESPACE

#endif
