/****************************************************************************
**
** Copyright (C) 2013 Research In Motion
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QNXAUDIOOUTPUT_H
#define QNXAUDIOOUTPUT_H

#include "qaudiosystem.h"

#include <QTime>
#include <QTimer>

#include <sys/asoundlib.h>

QT_BEGIN_NAMESPACE

class QnxPushIODevice;

class QnxAudioOutput : public QAbstractAudioOutput
{
    Q_OBJECT

public:
    QnxAudioOutput();
    ~QnxAudioOutput();

    void start(QIODevice *source) Q_DECL_OVERRIDE;
    QIODevice *start() Q_DECL_OVERRIDE;
    void stop() Q_DECL_OVERRIDE;
    void reset() Q_DECL_OVERRIDE;
    void suspend() Q_DECL_OVERRIDE;
    void resume() Q_DECL_OVERRIDE;
    int bytesFree() const Q_DECL_OVERRIDE;
    int periodSize() const Q_DECL_OVERRIDE;
    void setBufferSize(int) Q_DECL_OVERRIDE {}
    int bufferSize() const Q_DECL_OVERRIDE { return 0; }
    void setNotifyInterval(int ms) Q_DECL_OVERRIDE;
    int notifyInterval() const Q_DECL_OVERRIDE;
    qint64 processedUSecs() const Q_DECL_OVERRIDE;
    qint64 elapsedUSecs() const Q_DECL_OVERRIDE;
    QAudio::Error error() const Q_DECL_OVERRIDE;
    QAudio::State state() const Q_DECL_OVERRIDE;
    void setFormat(const QAudioFormat &format) Q_DECL_OVERRIDE;
    QAudioFormat format() const Q_DECL_OVERRIDE;
    void setVolume(qreal volume) Q_DECL_OVERRIDE;
    qreal volume() const Q_DECL_OVERRIDE;

private slots:
    void pullData();

private:
    bool open();
    void close();
    void setError(QAudio::Error error);
    void setState(QAudio::State state);

    friend class QnxPushIODevice;
    qint64 write(const char *data, qint64 len);

    QIODevice *m_source;
    bool m_pushSource;
    QTimer m_timer;

    int m_notifyInterval;
    QAudio::Error m_error;
    QAudio::State m_state;
    QAudioFormat m_format;
    qreal m_volume;
    int m_periodSize;

    snd_pcm_t *m_pcmHandle;
    qint64 m_bytesWritten;
    QTime m_startTimeStamp;
    QTime m_intervalTimeStamp;
    qint64 m_intervalOffset;
};

class QnxPushIODevice : public QIODevice
{
    Q_OBJECT
public:
    explicit QnxPushIODevice(QnxAudioOutput *output);
    ~QnxPushIODevice();

    qint64 readData(char *data, qint64 len);
    qint64 writeData(const char *data, qint64 len);

private:
    QnxAudioOutput *m_output;
};

QT_END_NAMESPACE

#endif
