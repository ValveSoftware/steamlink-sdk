/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

#ifndef QAUDIOINPUTWASAPI_H
#define QAUDIOINPUTWASAPI_H

#include <QtCore/QLoggingCategory>
#include <QtCore/QMutex>
#include <QtCore/QTime>
#include <QtMultimedia/QAbstractAudioInput>
#include <QtMultimedia/QAudio>

#include <wrl.h>

struct IAudioCaptureClient;
struct IAudioStreamVolume;

QT_BEGIN_NAMESPACE

class AudioInterface;
class WasapiInputDevicePrivate;
class QWasapiProcessThread;

Q_DECLARE_LOGGING_CATEGORY(lcMmAudioInput)

class QWasapiAudioInput : public QAbstractAudioInput
{
    Q_OBJECT
public:
    explicit QWasapiAudioInput(const QByteArray &device);
    ~QWasapiAudioInput();

    void start(QIODevice* device) Q_DECL_OVERRIDE;
    QIODevice* start() Q_DECL_OVERRIDE;
    void stop() Q_DECL_OVERRIDE;
    void reset() Q_DECL_OVERRIDE;
    void suspend() Q_DECL_OVERRIDE;
    void resume() Q_DECL_OVERRIDE;
    int bytesReady() const Q_DECL_OVERRIDE;
    int periodSize() const Q_DECL_OVERRIDE;
    void setBufferSize(int value) Q_DECL_OVERRIDE;
    int bufferSize() const Q_DECL_OVERRIDE;
    void setNotifyInterval(int milliSeconds) Q_DECL_OVERRIDE;
    int notifyInterval() const Q_DECL_OVERRIDE;
    qint64 processedUSecs() const Q_DECL_OVERRIDE;
    qint64 elapsedUSecs() const Q_DECL_OVERRIDE;
    QAudio::Error error() const Q_DECL_OVERRIDE;
    QAudio::State state() const Q_DECL_OVERRIDE;
    void setFormat(const QAudioFormat& fmt) Q_DECL_OVERRIDE;
    QAudioFormat format() const Q_DECL_OVERRIDE;
    void setVolume(qreal) Q_DECL_OVERRIDE;
    qreal volume() const Q_DECL_OVERRIDE;

    void process();
public slots:
    void processBuffer();
private:
    bool initStart(bool pull);
    friend class WasapiInputDevicePrivate;
    friend class WasapiInputPrivate;
    QByteArray m_deviceName;
    Microsoft::WRL::ComPtr<AudioInterface> m_interface;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> m_capture;
#if defined(CLASSIC_APP_BUILD) || _MSC_VER >= 1900
    Microsoft::WRL::ComPtr<IAudioStreamVolume> m_volumeControl;
    qreal m_volumeCache;
#endif
    QMutex m_mutex;
    QAudio::State m_currentState;
    QAudio::Error m_currentError;
    QAudioFormat m_currentFormat;
    qint64 m_bytesProcessed;
    QTime m_openTime;
    int m_openTimeOffset;
    int m_interval;
    bool m_pullMode;
    quint32 m_bufferFrames;
    quint32 m_bufferBytes;
    HANDLE m_event;
    QWasapiProcessThread *m_eventThread;
    QAtomicInt m_processing;
    QIODevice *m_eventDevice;
};

QT_END_NAMESPACE

#endif
