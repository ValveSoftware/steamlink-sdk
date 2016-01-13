/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#ifndef QOPENSLESAUDIOOUTPUT_H
#define QOPENSLESAUDIOOUTPUT_H

#include <qaudiosystem.h>
#include <SLES/OpenSLES.h>
#include <qbytearray.h>
#include <qmap.h>
#include <QTime>

QT_BEGIN_NAMESPACE

class QOpenSLESAudioOutput : public QAbstractAudioOutput
{
    Q_OBJECT

public:
    QOpenSLESAudioOutput(const QByteArray &device);
    ~QOpenSLESAudioOutput();

    void start(QIODevice *device) Q_DECL_OVERRIDE;
    QIODevice *start() Q_DECL_OVERRIDE;
    void stop() Q_DECL_OVERRIDE;
    void reset() Q_DECL_OVERRIDE;
    void suspend() Q_DECL_OVERRIDE;
    void resume() Q_DECL_OVERRIDE;
    int bytesFree() const Q_DECL_OVERRIDE;
    int periodSize() const Q_DECL_OVERRIDE;
    void setBufferSize(int value) Q_DECL_OVERRIDE;
    int bufferSize() const Q_DECL_OVERRIDE;
    void setNotifyInterval(int milliSeconds) Q_DECL_OVERRIDE;
    int notifyInterval() const Q_DECL_OVERRIDE;
    qint64 processedUSecs() const Q_DECL_OVERRIDE;
    qint64 elapsedUSecs() const Q_DECL_OVERRIDE;
    QAudio::Error error() const Q_DECL_OVERRIDE;
    QAudio::State state() const Q_DECL_OVERRIDE;
    void setFormat(const QAudioFormat &format) Q_DECL_OVERRIDE;
    QAudioFormat format() const Q_DECL_OVERRIDE;

    void setVolume(qreal volume) Q_DECL_OVERRIDE;
    qreal volume() const Q_DECL_OVERRIDE;

    void setCategory(const QString &category) Q_DECL_OVERRIDE;
    QString category() const Q_DECL_OVERRIDE;

private:
    friend class SLIODevicePrivate;

    Q_INVOKABLE void onEOSEvent();
    Q_INVOKABLE void onBytesProcessed(qint64 bytes);
    void bufferAvailable(quint32 count, quint32 playIndex);

    static void playCallback(SLPlayItf playItf, void *ctx, SLuint32 event);
    static void bufferQueueCallback(SLBufferQueueItf bufferQueue, void *ctx);

    bool preparePlayer();
    void destroyPlayer();
    void stopPlayer();
    void startPlayer();
    qint64 writeData(const char *data, qint64 len);

    void setState(QAudio::State state);
    void setError(QAudio::Error error);

    SLmillibel adjustVolume(qreal vol);

    QByteArray m_deviceName;
    QAudio::State m_state;
    QAudio::Error m_error;
    SLObjectItf m_outputMixObject;
    SLObjectItf m_playerObject;
    SLPlayItf m_playItf;
    SLVolumeItf m_volumeItf;
    SLBufferQueueItf m_bufferQueueItf;
    QIODevice *m_audioSource;
    char *m_buffers;
    qreal m_volume;
    bool m_pullMode;
    int m_nextBuffer;
    int m_bufferSize;
    int m_notifyInterval;
    int m_periodSize;
    qint64 m_elapsedTime;
    qint64 m_processedBytes;
    QAtomicInt m_availableBuffers;
    SLuint32 m_eventMask;
    bool m_startRequiresInit;

    qint32 m_streamType;
    QTime m_clockStamp;
    QAudioFormat m_format;
    QString m_category;
    static QMap<QString, qint32> m_categories;
};

class SLIODevicePrivate : public QIODevice
{
    Q_OBJECT

public:
    inline SLIODevicePrivate(QOpenSLESAudioOutput *audio) : m_audioDevice(audio) {}
    inline ~SLIODevicePrivate() Q_DECL_OVERRIDE {}

protected:
    inline qint64 readData(char *, qint64) Q_DECL_OVERRIDE { return 0; }
    inline qint64 writeData(const char *data, qint64 len) Q_DECL_OVERRIDE;

private:
    QOpenSLESAudioOutput *m_audioDevice;
};

qint64 SLIODevicePrivate::writeData(const char *data, qint64 len)
{
    Q_ASSERT(m_audioDevice);
    return m_audioDevice->writeData(data, len);
}

QT_END_NAMESPACE

#endif // QOPENSLESAUDIOOUTPUT_H
