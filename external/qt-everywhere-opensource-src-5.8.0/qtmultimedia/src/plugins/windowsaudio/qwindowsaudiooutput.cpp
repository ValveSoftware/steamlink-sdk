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
// INTERNAL USE ONLY: Do NOT use for any other purpose.
//

#include "qwindowsaudiooutput.h"
#include "qwindowsaudiodeviceinfo.h"
#include "qwindowsaudioutils.h"
#include <QtEndian>
#include <QtCore/QDataStream>
#include <private/qaudiohelpers_p.h>

//#define DEBUG_AUDIO 1

QT_BEGIN_NAMESPACE

QWindowsAudioOutput::QWindowsAudioOutput(const QByteArray &device)
{
    bytesAvailable = 0;
    buffer_size = 0;
    period_size = 0;
    m_device = device;
    totalTimeValue = 0;
    intervalTime = 1000;
    audioBuffer = 0;
    errorState = QAudio::NoError;
    deviceState = QAudio::StoppedState;
    audioSource = 0;
    pullMode = true;
    finished = false;
    volumeCache = qreal(1.0);
}

QWindowsAudioOutput::~QWindowsAudioOutput()
{
    mutex.lock();
    finished = true;
    mutex.unlock();

    close();
}

void CALLBACK QWindowsAudioOutput::waveOutProc( HWAVEOUT hWaveOut, UINT uMsg,
        DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2 )
{
    Q_UNUSED(dwParam1)
    Q_UNUSED(dwParam2)
    Q_UNUSED(hWaveOut)

    QWindowsAudioOutput* qAudio;
    qAudio = (QWindowsAudioOutput*)(dwInstance);
    if(!qAudio)
        return;

    QMutexLocker locker(&qAudio->mutex);

    switch(uMsg) {
        case WOM_OPEN:
            qAudio->feedback();
            break;
        case WOM_CLOSE:
            return;
        case WOM_DONE:
            if(qAudio->finished || qAudio->buffer_size == 0 || qAudio->period_size == 0) {
                return;
            }
            qAudio->waveFreeBlockCount++;
            if(qAudio->waveFreeBlockCount >= qAudio->buffer_size/qAudio->period_size)
                qAudio->waveFreeBlockCount = qAudio->buffer_size/qAudio->period_size;
            qAudio->feedback();
            break;
        default:
            return;
    }
}

WAVEHDR* QWindowsAudioOutput::allocateBlocks(int size, int count)
{
    int i;
    unsigned char* buffer;
    WAVEHDR* blocks;
    DWORD totalBufferSize = (size + sizeof(WAVEHDR))*count;

    if((buffer=(unsigned char*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,
                    totalBufferSize)) == 0) {
        qWarning("QAudioOutput: Memory allocation error");
        return 0;
    }
    blocks = (WAVEHDR*)buffer;
    buffer += sizeof(WAVEHDR)*count;
    for(i = 0; i < count; i++) {
        blocks[i].dwBufferLength = size;
        blocks[i].lpData = (LPSTR)buffer;
        buffer += size;
    }
    return blocks;
}

void QWindowsAudioOutput::freeBlocks(WAVEHDR* blockArray)
{
    WAVEHDR* blocks = blockArray;

    int count = buffer_size/period_size;

    for(int i = 0; i < count; i++) {
        waveOutUnprepareHeader(hWaveOut,blocks, sizeof(WAVEHDR));
        blocks++;
    }
    HeapFree(GetProcessHeap(), 0, blockArray);
}

QAudioFormat QWindowsAudioOutput::format() const
{
    return settings;
}

void QWindowsAudioOutput::setFormat(const QAudioFormat& fmt)
{
    if (deviceState == QAudio::StoppedState)
        settings = fmt;
}

void QWindowsAudioOutput::start(QIODevice* device)
{
    if(deviceState != QAudio::StoppedState)
        close();

    if(!pullMode && audioSource)
        delete audioSource;

    pullMode = true;
    audioSource = device;

    deviceState = QAudio::ActiveState;

    if(!open())
        return;

    emit stateChanged(deviceState);
}

QIODevice* QWindowsAudioOutput::start()
{
    if(deviceState != QAudio::StoppedState)
        close();

    if(!pullMode && audioSource)
        delete audioSource;

    pullMode = false;
    audioSource = new OutputPrivate(this);
    audioSource->open(QIODevice::WriteOnly|QIODevice::Unbuffered);

    deviceState = QAudio::IdleState;

    if(!open())
        return 0;

    emit stateChanged(deviceState);

    return audioSource;
}

void QWindowsAudioOutput::stop()
{
    if(deviceState == QAudio::StoppedState)
        return;
    close();
    if(!pullMode && audioSource) {
        delete audioSource;
        audioSource = 0;
    }
    emit stateChanged(deviceState);
}

bool QWindowsAudioOutput::open()
{
#ifdef DEBUG_AUDIO
    QTime now(QTime::currentTime());
    qDebug()<<now.second()<<"s "<<now.msec()<<"ms :open()";
#endif

    period_size = 0;

    if (!qt_convertFormat(settings, &wfx)) {
        qWarning("QAudioOutput: open error, invalid format.");
    } else if (buffer_size == 0) {
        // Default buffer size, 200ms, default period size is 40ms
        buffer_size
                = (settings.sampleRate()
                * settings.channelCount()
                * settings.sampleSize()
                + 39) / 40;
        period_size = buffer_size / 5;
    } else {
        period_size = buffer_size / 5;
    }

    if (period_size == 0) {
        errorState = QAudio::OpenError;
        deviceState = QAudio::StoppedState;
        emit stateChanged(deviceState);
        return false;
    }

    waveBlocks = allocateBlocks(period_size, buffer_size/period_size);

    mutex.lock();
    waveFreeBlockCount = buffer_size/period_size;
    mutex.unlock();

    waveCurrentBlock = 0;

    if(audioBuffer == 0)
        audioBuffer = new char[buffer_size];

    timeStamp.restart();
    elapsedTimeOffset = 0;

    QDataStream ds(&m_device, QIODevice::ReadOnly);
    quint32 deviceId;
    ds >> deviceId;

    if (waveOutOpen(&hWaveOut, UINT_PTR(deviceId), &wfx.Format,
                    (DWORD_PTR)&waveOutProc,
                    (DWORD_PTR) this,
                    CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        errorState = QAudio::OpenError;
        deviceState = QAudio::StoppedState;
        emit stateChanged(deviceState);
        qWarning("QAudioOutput: open error");
        return false;
    }

    totalTimeValue = 0;
    timeStampOpened.restart();
    elapsedTimeOffset = 0;

    errorState = QAudio::NoError;
    if(pullMode) {
        deviceState = QAudio::ActiveState;
        QTimer::singleShot(10, this, SLOT(feedback()));
    } else
        deviceState = QAudio::IdleState;

    return true;
}

void QWindowsAudioOutput::close()
{
    if(deviceState == QAudio::StoppedState)
        return;

    deviceState = QAudio::StoppedState;
    errorState = QAudio::NoError;
    int delay = (buffer_size-bytesFree())*1000/(settings.sampleRate()
                  *settings.channelCount()*(settings.sampleSize()/8));
    waveOutReset(hWaveOut);
    Sleep(delay+10);

    freeBlocks(waveBlocks);
    waveOutClose(hWaveOut);
    delete [] audioBuffer;
    audioBuffer = 0;
    buffer_size = 0;
}

int QWindowsAudioOutput::bytesFree() const
{
    int buf;
    buf = waveFreeBlockCount*period_size;

    return buf;
}

int QWindowsAudioOutput::periodSize() const
{
    return period_size;
}

void QWindowsAudioOutput::setBufferSize(int value)
{
    if(deviceState == QAudio::StoppedState)
        buffer_size = value;
}

int QWindowsAudioOutput::bufferSize() const
{
    return buffer_size;
}

void QWindowsAudioOutput::setNotifyInterval(int ms)
{
    intervalTime = qMax(0, ms);
}

int QWindowsAudioOutput::notifyInterval() const
{
    return intervalTime;
}

qint64 QWindowsAudioOutput::processedUSecs() const
{
    if (deviceState == QAudio::StoppedState)
        return 0;
    qint64 result = qint64(1000000) * totalTimeValue /
        (settings.channelCount()*(settings.sampleSize()/8)) /
        settings.sampleRate();

    return result;
}

qint64 QWindowsAudioOutput::write( const char *data, qint64 len )
{
    // Write out some audio data
    if (deviceState != QAudio::ActiveState && deviceState != QAudio::IdleState)
        return 0;

    char* p = (char*)data;
    int l = (int)len;

    QByteArray reverse;
    if (settings.byteOrder() == QAudioFormat::BigEndian) {

        switch (settings.sampleSize()) {
            case 8:
                // No need to convert
                break;

            case 16:
                reverse.resize(l);
                for (qint64 i = 0; i < (l >> 1); i++)
                    *((qint16*)reverse.data() + i) = qFromBigEndian(*((qint16*)data + i));
                p = reverse.data();
                break;

            case 32:
                reverse.resize(l);
                for (qint64 i = 0; i < (l >> 2); i++)
                    *((qint32*)reverse.data() + i) = qFromBigEndian(*((qint32*)data + i));
                p = reverse.data();
                break;
        }
    }

    WAVEHDR* current;
    int remain;
    current = &waveBlocks[waveCurrentBlock];
    while(l > 0) {
        mutex.lock();
        if(waveFreeBlockCount==0) {
            mutex.unlock();
            break;
        }
        mutex.unlock();

        if(current->dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(hWaveOut, current, sizeof(WAVEHDR));

        if(l < period_size)
            remain = l;
        else
            remain = period_size;

        if (volumeCache < qreal(1.0))
            QAudioHelperInternal::qMultiplySamples(volumeCache, settings, p, current->lpData, remain);
        else
            memcpy(current->lpData, p, remain);

        l -= remain;
        p += remain;
        current->dwBufferLength = remain;
        waveOutPrepareHeader(hWaveOut, current, sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, current, sizeof(WAVEHDR));

        mutex.lock();
        waveFreeBlockCount--;
#ifdef DEBUG_AUDIO
        qDebug("write out l=%d, waveFreeBlockCount=%d",
                current->dwBufferLength,waveFreeBlockCount);
#endif
        mutex.unlock();

        totalTimeValue += current->dwBufferLength;
        waveCurrentBlock++;
        waveCurrentBlock %= buffer_size/period_size;
        current = &waveBlocks[waveCurrentBlock];
        current->dwUser = 0;
        errorState = QAudio::NoError;
        if (deviceState != QAudio::ActiveState) {
            deviceState = QAudio::ActiveState;
            emit stateChanged(deviceState);
        }
    }
    return (len-l);
}

void QWindowsAudioOutput::resume()
{
    if(deviceState == QAudio::SuspendedState) {
        deviceState = pullMode ? QAudio::ActiveState : QAudio::IdleState;
        errorState = QAudio::NoError;
        waveOutRestart(hWaveOut);
        QTimer::singleShot(10, this, SLOT(feedback()));
        emit stateChanged(deviceState);
    }
}

void QWindowsAudioOutput::suspend()
{
    if(deviceState == QAudio::ActiveState || deviceState == QAudio::IdleState) {
        int delay = (buffer_size-bytesFree())*1000/(settings.sampleRate()
                *settings.channelCount()*(settings.sampleSize()/8));
        waveOutPause(hWaveOut);
        Sleep(delay+10);
        deviceState = QAudio::SuspendedState;
        errorState = QAudio::NoError;
        emit stateChanged(deviceState);
    }
}

void QWindowsAudioOutput::feedback()
{
#ifdef DEBUG_AUDIO
    QTime now(QTime::currentTime());
    qDebug()<<now.second()<<"s "<<now.msec()<<"ms :feedback()";
#endif
    bytesAvailable = bytesFree();

    if(!(deviceState==QAudio::StoppedState||deviceState==QAudio::SuspendedState)) {
        if(bytesAvailable >= period_size)
            QMetaObject::invokeMethod(this, "deviceReady", Qt::QueuedConnection);
    }
}

bool QWindowsAudioOutput::deviceReady()
{
    if(deviceState == QAudio::StoppedState || deviceState == QAudio::SuspendedState)
        return false;

    if(pullMode) {
        int chunks = bytesAvailable/period_size;
#ifdef DEBUG_AUDIO
        qDebug()<<"deviceReady() avail="<<bytesAvailable<<" bytes, period size="<<period_size<<" bytes";
        qDebug()<<"deviceReady() no. of chunks that can fit ="<<chunks<<", chunks in bytes ="<<chunks*period_size;
#endif
        bool startup = false;
        if(totalTimeValue == 0)
            startup = true;

        bool full=false;

        mutex.lock();
        if (waveFreeBlockCount==0) full = true;
        mutex.unlock();

        if (full) {
#ifdef DEBUG_AUDIO
            qDebug() << "Skipping data as unable to write";
#endif
            if ((timeStamp.elapsed() + elapsedTimeOffset) > intervalTime) {
                emit notify();
                elapsedTimeOffset = timeStamp.elapsed() + elapsedTimeOffset - intervalTime;
                timeStamp.restart();
            }
            return true;
        }

        if(startup)
            waveOutPause(hWaveOut);
        int input = period_size*chunks;
        int l = audioSource->read(audioBuffer,input);
        if(l > 0) {
            int out= write(audioBuffer,l);
            if(out > 0) {
                if (deviceState != QAudio::ActiveState) {
                    deviceState = QAudio::ActiveState;
                    emit stateChanged(deviceState);
                }
            }
            if ( out < l) {
                // Didn't write all data
                audioSource->seek(audioSource->pos()-(l-out));
            }
            if (startup)
                waveOutRestart(hWaveOut);
        } else if(l == 0) {
            bytesAvailable = bytesFree();

            int check = 0;

            mutex.lock();
            check = waveFreeBlockCount;
            mutex.unlock();

            if(check == buffer_size/period_size) {
                if (deviceState != QAudio::IdleState) {
                    errorState = QAudio::UnderrunError;
                    deviceState = QAudio::IdleState;
                    emit stateChanged(deviceState);
                }
            }

        } else if(l < 0) {
            bytesAvailable = bytesFree();
            if (errorState != QAudio::IOError)
                errorState = QAudio::IOError;
        }
    } else {
        int buffered;

        mutex.lock();
        buffered = waveFreeBlockCount;
        mutex.unlock();

        if (buffered >= buffer_size/period_size && deviceState == QAudio::ActiveState) {
            if (deviceState != QAudio::IdleState) {
                errorState = QAudio::UnderrunError;
                deviceState = QAudio::IdleState;
                emit stateChanged(deviceState);
            }
        }
    }
    if(deviceState != QAudio::ActiveState && deviceState != QAudio::IdleState)
        return true;

    if(intervalTime && (timeStamp.elapsed() + elapsedTimeOffset) > intervalTime) {
        emit notify();
        elapsedTimeOffset = timeStamp.elapsed() + elapsedTimeOffset - intervalTime;
        timeStamp.restart();
    }

    return true;
}

qint64 QWindowsAudioOutput::elapsedUSecs() const
{
    if (deviceState == QAudio::StoppedState)
        return 0;

    return timeStampOpened.elapsed() * qint64(1000);
}

QAudio::Error QWindowsAudioOutput::error() const
{
    return errorState;
}

QAudio::State QWindowsAudioOutput::state() const
{
    return deviceState;
}

void QWindowsAudioOutput::setVolume(qreal v)
{
    if (qFuzzyCompare(volumeCache, v))
        return;

    volumeCache = qBound(qreal(0), v, qreal(1));
}

qreal QWindowsAudioOutput::volume() const
{
    return volumeCache;
}

void QWindowsAudioOutput::reset()
{
    close();
}

OutputPrivate::OutputPrivate(QWindowsAudioOutput* audio)
{
    audioDevice = qobject_cast<QWindowsAudioOutput*>(audio);
}

OutputPrivate::~OutputPrivate() {}

qint64 OutputPrivate::readData( char* data, qint64 len)
{
    Q_UNUSED(data)
    Q_UNUSED(len)

    return 0;
}

qint64 OutputPrivate::writeData(const char* data, qint64 len)
{
    int retry = 0;
    qint64 written = 0;

    if((audioDevice->deviceState == QAudio::ActiveState)
            ||(audioDevice->deviceState == QAudio::IdleState)) {
        qint64 l = len;
        while(written < l) {
            int chunk = audioDevice->write(data+written,(l-written));
            if(chunk <= 0)
                retry++;
            else
                written+=chunk;

            if(retry > 10)
                return written;
        }
        audioDevice->deviceState = QAudio::ActiveState;
    }
    return written;
}

QT_END_NAMESPACE

#include "moc_qwindowsaudiooutput.cpp"
