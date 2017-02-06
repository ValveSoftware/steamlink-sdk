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

#ifndef QAUDIOOUTPUTPULSE_H
#define QAUDIOOUTPUTPULSE_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

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

class QPulseAudioOutput : public QAbstractAudioOutput
{
    friend class PulseOutputPrivate;
    Q_OBJECT

public:
    QPulseAudioOutput(const QByteArray &device);
    ~QPulseAudioOutput();

    void start(QIODevice *device);
    QIODevice *start();
    void stop();
    void reset();
    void suspend();
    void resume();
    int bytesFree() const;
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

    void setCategory(const QString &category);
    QString category() const;

public:
    void streamUnderflowCallback();

private:
    void setState(QAudio::State state);
    void setError(QAudio::Error error);

    bool open();
    void close();
    qint64 write(const char *data, qint64 len);

private Q_SLOTS:
    void userFeed();
    void onPulseContextFailed();

private:
    QByteArray m_device;
    QByteArray m_streamName;
    QAudioFormat m_format;
    QAudio::Error m_errorState;
    QAudio::State m_deviceState;
    bool m_pullMode;
    bool m_opened;
    QIODevice *m_audioSource;
    QTimer m_periodTimer;
    int m_periodTime;
    pa_stream *m_stream;
    int m_notifyInterval;
    int m_periodSize;
    int m_bufferSize;
    int m_maxBufferSize;
    QTime m_clockStamp;
    qint64 m_totalTimeValue;
    QTimer *m_tickTimer;
    char *m_audioBuffer;
    QTime m_timeStamp;
    qint64 m_elapsedTimeOffset;
    bool m_resuming;
    QString m_category;

    qreal m_volume;
    pa_sample_spec m_spec;
};

class PulseOutputPrivate : public QIODevice
{
    friend class QPulseAudioOutput;
    Q_OBJECT

public:
    PulseOutputPrivate(QPulseAudioOutput *audio);
    virtual ~PulseOutputPrivate() {}

protected:
    qint64 readData(char *data, qint64 len);
    qint64 writeData(const char *data, qint64 len);

private:
    QPulseAudioOutput *m_audioDevice;
};

QT_END_NAMESPACE

#endif
