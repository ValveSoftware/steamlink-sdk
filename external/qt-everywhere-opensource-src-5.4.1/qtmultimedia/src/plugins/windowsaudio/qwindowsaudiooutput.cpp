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
#include <QtEndian>

#ifndef SPEAKER_FRONT_LEFT
    #define SPEAKER_FRONT_LEFT            0x00000001
    #define SPEAKER_FRONT_RIGHT           0x00000002
    #define SPEAKER_FRONT_CENTER          0x00000004
    #define SPEAKER_LOW_FREQUENCY         0x00000008
    #define SPEAKER_BACK_LEFT             0x00000010
    #define SPEAKER_BACK_RIGHT            0x00000020
    #define SPEAKER_FRONT_LEFT_OF_CENTER  0x00000040
    #define SPEAKER_FRONT_RIGHT_OF_CENTER 0x00000080
    #define SPEAKER_BACK_CENTER           0x00000100
    #define SPEAKER_SIDE_LEFT             0x00000200
    #define SPEAKER_SIDE_RIGHT            0x00000400
    #define SPEAKER_TOP_CENTER            0x00000800
    #define SPEAKER_TOP_FRONT_LEFT        0x00001000
    #define SPEAKER_TOP_FRONT_CENTER      0x00002000
    #define SPEAKER_TOP_FRONT_RIGHT       0x00004000
    #define SPEAKER_TOP_BACK_LEFT         0x00008000
    #define SPEAKER_TOP_BACK_CENTER       0x00010000
    #define SPEAKER_TOP_BACK_RIGHT        0x00020000
    #define SPEAKER_RESERVED              0x7FFC0000
    #define SPEAKER_ALL                   0x80000000
#endif

#ifndef _WAVEFORMATEXTENSIBLE_

    #define _WAVEFORMATEXTENSIBLE_
    typedef struct
    {
        WAVEFORMATEX Format;          // Base WAVEFORMATEX data
        union
        {
            WORD wValidBitsPerSample; // Valid bits in each sample container
            WORD wSamplesPerBlock;    // Samples per block of audio data; valid
                                      // if wBitsPerSample=0 (but rarely used).
            WORD wReserved;           // Zero if neither case above applies.
        } Samples;
        DWORD dwChannelMask;          // Positions of the audio channels
        GUID SubFormat;               // Format identifier GUID
    } WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE, *LPPWAVEFORMATEXTENSIBLE;
    typedef const WAVEFORMATEXTENSIBLE* LPCWAVEFORMATEXTENSIBLE;

#endif

#if !defined(WAVE_FORMAT_EXTENSIBLE)
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#endif

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
    volumeCache = (qreal)1.;
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

    QMutexLocker(&qAudio->mutex);

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

    if (!settings.isValid()) {
        qWarning("QAudioOutput: open error, invalid format.");
    } else if (settings.channelCount() <= 0) {
        qWarning("QAudioOutput: open error, invalid number of channels (%d).",
                 settings.channelCount());
    } else if (settings.sampleSize() <= 0) {
        qWarning("QAudioOutput: open error, invalid sample size (%d).",
                 settings.sampleSize());
    } else if (settings.sampleRate() < 8000 || settings.sampleRate() > 96000) {
        qWarning("QAudioOutput: open error, sample rate out of range (%d).", settings.sampleRate());
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

    wfx.nSamplesPerSec = settings.sampleRate();
    wfx.wBitsPerSample = settings.sampleSize();
    wfx.nChannels = settings.channelCount();
    wfx.cbSize = 0;

    bool surround = false;

    if (settings.channelCount() > 2)
        surround = true;

    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nBlockAlign = (wfx.wBitsPerSample >> 3) * wfx.nChannels;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

    QDataStream ds(&m_device, QIODevice::ReadOnly);
    quint32 deviceId;
    ds >> deviceId;

    if (!surround) {
        if (waveOutOpen(&hWaveOut, UINT_PTR(deviceId), &wfx,
                    (DWORD_PTR)&waveOutProc,
                    (DWORD_PTR) this,
                    CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
            errorState = QAudio::OpenError;
            deviceState = QAudio::StoppedState;
            emit stateChanged(deviceState);
            qWarning("QAudioOutput: open error");
            return false;
        }
    } else {
        WAVEFORMATEXTENSIBLE wfex;
        wfex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wfex.Format.nChannels = settings.channelCount();
        wfex.Format.wBitsPerSample = settings.sampleSize();
        wfex.Format.nSamplesPerSec = settings.sampleRate();
        wfex.Format.nBlockAlign = wfex.Format.nChannels*wfex.Format.wBitsPerSample/8;
        wfex.Format.nAvgBytesPerSec=wfex.Format.nSamplesPerSec*wfex.Format.nBlockAlign;
        wfex.Samples.wValidBitsPerSample=wfex.Format.wBitsPerSample;
        static const GUID _KSDATAFORMAT_SUBTYPE_PCM = {
             0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
        wfex.SubFormat=_KSDATAFORMAT_SUBTYPE_PCM;
        wfex.Format.cbSize=22;

        wfex.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
        if (settings.channelCount() >= 4)
            wfex.dwChannelMask |= SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
        if (settings.channelCount() >= 6)
            wfex.dwChannelMask |= SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY;
        if (settings.channelCount() == 8)
            wfex.dwChannelMask |= SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;

        if (waveOutOpen(&hWaveOut, UINT_PTR(deviceId), &wfex.Format,
                    (DWORD_PTR)&waveOutProc,
                    (DWORD_PTR) this,
                    CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
            errorState = QAudio::OpenError;
            deviceState = QAudio::StoppedState;
            emit stateChanged(deviceState);
            qWarning("QAudioOutput: open error");
            return false;
        }
    }

    totalTimeValue = 0;
    timeStampOpened.restart();
    elapsedTimeOffset = 0;

    setVolume(volumeCache);

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
        deviceState = QAudio::ActiveState;
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
    const qreal normalizedVolume = qBound(qreal(0.0), v, qreal(1.0));
    if (deviceState != QAudio::ActiveState) {
        volumeCache = normalizedVolume;
        return;
    }
    const quint16 scaled = normalizedVolume * 0xFFFF;
    DWORD vol = MAKELONG(scaled, scaled);
    MMRESULT res = waveOutSetVolume(hWaveOut, vol);
    if (res == MMSYSERR_NOERROR)
        volumeCache = normalizedVolume;
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
