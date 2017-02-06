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


#include "qwindowsaudioinput.h"

#include <QtCore/QDataStream>

QT_BEGIN_NAMESPACE

//#define DEBUG_AUDIO 1

QWindowsAudioInput::QWindowsAudioInput(const QByteArray &device)
{
    bytesAvailable = 0;
    buffer_size = 0;
    period_size = 0;
    m_device = device;
    totalTimeValue = 0;
    intervalTime = 1000;
    errorState = QAudio::NoError;
    deviceState = QAudio::StoppedState;
    audioSource = 0;
    pullMode = true;
    resuming = false;
    finished = false;
    waveBlockOffset = 0;

    mixerID = 0;
    memset(&mixerLineControls, 0, sizeof(mixerLineControls));
    initMixer();
}

QWindowsAudioInput::~QWindowsAudioInput()
{
    stop();
    closeMixer();
}

void QT_WIN_CALLBACK QWindowsAudioInput::waveInProc( HWAVEIN hWaveIn, UINT uMsg,
        DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2 )
{
    Q_UNUSED(dwParam1)
    Q_UNUSED(dwParam2)
    Q_UNUSED(hWaveIn)

    QWindowsAudioInput* qAudio;
    qAudio = (QWindowsAudioInput*)(dwInstance);
    if(!qAudio)
        return;

    QMutexLocker locker(&qAudio->mutex);

    switch(uMsg) {
        case WIM_OPEN:
            break;
        case WIM_DATA:
            if(qAudio->waveFreeBlockCount > 0)
                qAudio->waveFreeBlockCount--;
            qAudio->feedback();
            break;
        case WIM_CLOSE:
            qAudio->finished = true;
            break;
        default:
            return;
    }
}

WAVEHDR* QWindowsAudioInput::allocateBlocks(int size, int count)
{
    int i;
    unsigned char* buffer;
    WAVEHDR* blocks;
    DWORD totalBufferSize = (size + sizeof(WAVEHDR))*count;

    if((buffer=(unsigned char*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,
                    totalBufferSize)) == 0) {
        qWarning("QAudioInput: Memory allocation error");
        return 0;
    }
    blocks = (WAVEHDR*)buffer;
    buffer += sizeof(WAVEHDR)*count;
    for(i = 0; i < count; i++) {
        blocks[i].dwBufferLength = size;
        blocks[i].lpData = (LPSTR)buffer;
        blocks[i].dwBytesRecorded=0;
        blocks[i].dwUser = 0L;
        blocks[i].dwFlags = 0L;
        blocks[i].dwLoops = 0L;
        result = waveInPrepareHeader(hWaveIn,&blocks[i], sizeof(WAVEHDR));
        if(result != MMSYSERR_NOERROR) {
            qWarning("QAudioInput: Can't prepare block %d",i);
            return 0;
        }
        buffer += size;
    }
    return blocks;
}

void QWindowsAudioInput::freeBlocks(WAVEHDR* blockArray)
{
    WAVEHDR* blocks = blockArray;

    int count = buffer_size/period_size;

    for(int i = 0; i < count; i++) {
        waveInUnprepareHeader(hWaveIn,blocks, sizeof(WAVEHDR));
        blocks++;
    }
    HeapFree(GetProcessHeap(), 0, blockArray);
}

QAudio::Error QWindowsAudioInput::error() const
{
    return errorState;
}

QAudio::State QWindowsAudioInput::state() const
{
    return deviceState;
}

#ifndef  DRVM_MAPPER_CONSOLEVOICECOM_GET
    #ifndef DRVM_MAPPER
    #define DRVM_MAPPER                     0x2000
    #endif
    #ifndef DRVM_MAPPER_STATUS
    #define DRVM_MAPPER_STATUS      (DRVM_MAPPER+0)
    #endif
    #define DRVM_USER                       0x4000
    #define DRVM_MAPPER_RECONFIGURE         (DRVM_MAPPER+1)
    #define DRVM_MAPPER_PREFERRED_GET       (DRVM_MAPPER+21)
    #define DRVM_MAPPER_CONSOLEVOICECOM_GET (DRVM_MAPPER+23)
#endif

void QWindowsAudioInput::setVolume(qreal volume)
{
    for (DWORD i = 0; i < mixerLineControls.cControls; i++) {

        MIXERCONTROLDETAILS controlDetails;
        controlDetails.cbStruct = sizeof(MIXERCONTROLDETAILS);
        controlDetails.dwControlID = mixerLineControls.pamxctrl[i].dwControlID;
        controlDetails.cChannels = 1;

        if ((mixerLineControls.pamxctrl[i].dwControlType == MIXERCONTROL_CONTROLTYPE_FADER) ||
            (mixerLineControls.pamxctrl[i].dwControlType == MIXERCONTROL_CONTROLTYPE_VOLUME)) {
            MIXERCONTROLDETAILS_UNSIGNED controlDetailsUnsigned;
            controlDetailsUnsigned.dwValue = qBound(DWORD(0), DWORD(65535.0 * volume + 0.5), DWORD(65535));
            controlDetails.cMultipleItems = 0;
            controlDetails.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
            controlDetails.paDetails = &controlDetailsUnsigned;
            mixerSetControlDetails(mixerID, &controlDetails, MIXER_SETCONTROLDETAILSF_VALUE);
        }
    }
}

qreal QWindowsAudioInput::volume() const
{
    DWORD volume = 0;
    for (DWORD i = 0; i < mixerLineControls.cControls; i++) {
        if ((mixerLineControls.pamxctrl[i].dwControlType != MIXERCONTROL_CONTROLTYPE_FADER) &&
            (mixerLineControls.pamxctrl[i].dwControlType != MIXERCONTROL_CONTROLTYPE_VOLUME)) {
            continue;
        }

        MIXERCONTROLDETAILS controlDetails;
        controlDetails.cbStruct = sizeof(controlDetails);
        controlDetails.dwControlID = mixerLineControls.pamxctrl[i].dwControlID;
        controlDetails.cChannels = 1;
        controlDetails.cMultipleItems = 0;
        controlDetails.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
        MIXERCONTROLDETAILS_UNSIGNED detailsUnsigned;
        controlDetails.paDetails = &detailsUnsigned;
        memset(controlDetails.paDetails, 0, controlDetails.cbDetails);

        MMRESULT result = mixerGetControlDetails(mixerID, &controlDetails, MIXER_GETCONTROLDETAILSF_VALUE);
        if (result != MMSYSERR_NOERROR)
            continue;
        if (controlDetails.cbDetails < sizeof(MIXERCONTROLDETAILS_UNSIGNED))
            continue;
        volume = detailsUnsigned.dwValue;
        break;
    }

    return volume / 65535.0;
}

void QWindowsAudioInput::setFormat(const QAudioFormat& fmt)
{
    if (deviceState == QAudio::StoppedState)
        settings = fmt;
}

QAudioFormat QWindowsAudioInput::format() const
{
    return settings;
}

void QWindowsAudioInput::start(QIODevice* device)
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

QIODevice* QWindowsAudioInput::start()
{
    if(deviceState != QAudio::StoppedState)
        close();

    if(!pullMode && audioSource)
        delete audioSource;

    pullMode = false;
    audioSource = new InputPrivate(this);
    audioSource->open(QIODevice::ReadOnly | QIODevice::Unbuffered);

    deviceState = QAudio::IdleState;

    if(!open())
        return 0;

    emit stateChanged(deviceState);

    return audioSource;
}

void QWindowsAudioInput::stop()
{
    if(deviceState == QAudio::StoppedState)
        return;

    close();
    emit stateChanged(deviceState);
}

bool QWindowsAudioInput::open()
{
#ifdef DEBUG_AUDIO
    QTime now(QTime::currentTime());
    qDebug()<<now.second()<<"s "<<now.msec()<<"ms :open()";
#endif
    header = 0;

    period_size = 0;

    if (!qt_convertFormat(settings, &wfx)) {
        qWarning("QAudioInput: open error, invalid format.");
    } else if (buffer_size == 0) {
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

    timeStamp.restart();
    elapsedTimeOffset = 0;

    QDataStream ds(&m_device, QIODevice::ReadOnly);
    quint32 deviceId;
    ds >> deviceId;

    if (waveInOpen(&hWaveIn, UINT_PTR(deviceId), &wfx.Format,
                (DWORD_PTR)&waveInProc,
                (DWORD_PTR) this,
                CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        errorState = QAudio::OpenError;
        deviceState = QAudio::StoppedState;
        emit stateChanged(deviceState);
        qWarning("QAudioInput: failed to open audio device");
        return false;
    }
    waveBlocks = allocateBlocks(period_size, buffer_size/period_size);
    waveBlockOffset = 0;

    if(waveBlocks == 0) {
        errorState = QAudio::OpenError;
        deviceState = QAudio::StoppedState;
        emit stateChanged(deviceState);
        qWarning("QAudioInput: failed to allocate blocks. open failed");
        return false;
    }

    mutex.lock();
    waveFreeBlockCount = buffer_size/period_size;
    mutex.unlock();

    for(int i=0; i<buffer_size/period_size; i++) {
        result = waveInAddBuffer(hWaveIn, &waveBlocks[i], sizeof(WAVEHDR));
        if(result != MMSYSERR_NOERROR) {
            qWarning("QAudioInput: failed to setup block %d,err=%d",i,result);
            errorState = QAudio::OpenError;
            deviceState = QAudio::StoppedState;
            emit stateChanged(deviceState);
            return false;
        }
    }
    result = waveInStart(hWaveIn);
    if(result) {
        qWarning("QAudioInput: failed to start audio input");
        errorState = QAudio::OpenError;
        deviceState = QAudio::StoppedState;
        emit stateChanged(deviceState);
        return false;
    }
    timeStampOpened.restart();
    elapsedTimeOffset = 0;
    totalTimeValue = 0;
    errorState  = QAudio::NoError;
    return true;
}

void QWindowsAudioInput::close()
{
    if(deviceState == QAudio::StoppedState)
        return;

    deviceState = QAudio::StoppedState;
    waveInReset(hWaveIn);

    mutex.lock();
    for (int i=0; i<waveFreeBlockCount; i++)
        waveInUnprepareHeader(hWaveIn,&waveBlocks[i],sizeof(WAVEHDR));
    freeBlocks(waveBlocks);
    mutex.unlock();

    waveInClose(hWaveIn);

    int count = 0;
    while(!finished && count < 500) {
        count++;
        Sleep(10);
    }
}

void QWindowsAudioInput::initMixer()
{
    QDataStream ds(&m_device, QIODevice::ReadOnly);
    quint32 inputDevice;
    ds >> inputDevice;

    if (int(inputDevice) < 0)
        return;

    // Get the Mixer ID from the Sound Device ID
    UINT mixerIntID = 0;
    if (mixerGetID((HMIXEROBJ)(quintptr(inputDevice)), &mixerIntID, MIXER_OBJECTF_WAVEIN) != MMSYSERR_NOERROR)
        return;
    mixerID = (HMIXEROBJ)mixerIntID;

    // Get the Destination (Recording) Line Information
    MIXERLINE mixerLine;
    mixerLine.cbStruct = sizeof(MIXERLINE);
    mixerLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_WAVEIN;
    if (mixerGetLineInfo(mixerID, &mixerLine, MIXER_GETLINEINFOF_COMPONENTTYPE) != MMSYSERR_NOERROR)
        return;

    // Set all the Destination (Recording) Line Controls
    if (mixerLine.cControls > 0) {
        mixerLineControls.cbStruct = sizeof(MIXERLINECONTROLS);
        mixerLineControls.dwLineID = mixerLine.dwLineID;
        mixerLineControls.cControls = mixerLine.cControls;
        mixerLineControls.cbmxctrl = sizeof(MIXERCONTROL);
        mixerLineControls.pamxctrl = new MIXERCONTROL[mixerLineControls.cControls];
        if (mixerGetLineControls(mixerID, &mixerLineControls, MIXER_GETLINECONTROLSF_ALL) != MMSYSERR_NOERROR)
            closeMixer();
    }
}

void QWindowsAudioInput::closeMixer()
{
    delete[] mixerLineControls.pamxctrl;
    memset(&mixerLineControls, 0, sizeof(mixerLineControls));
}

int QWindowsAudioInput::bytesReady() const
{
    if(period_size == 0 || buffer_size == 0)
        return 0;

    int buf = ((buffer_size/period_size)-waveFreeBlockCount)*period_size;
    if(buf < 0)
        buf = 0;
    return buf;
}

qint64 QWindowsAudioInput::read(char* data, qint64 len)
{
    bool done = false;

    char*  p = data;
    qint64 l = 0;
    qint64 written = 0;
    while(!done) {
        // Read in some audio data
        if(waveBlocks[header].dwBytesRecorded > 0 && waveBlocks[header].dwFlags & WHDR_DONE) {
            if(pullMode) {
                l = audioSource->write(waveBlocks[header].lpData + waveBlockOffset,
                        waveBlocks[header].dwBytesRecorded - waveBlockOffset);
#ifdef DEBUG_AUDIO
                qDebug()<<"IN: "<<waveBlocks[header].dwBytesRecorded<<", OUT: "<<l;
#endif
                if(l < 0) {
                    // error
                    qWarning("QAudioInput: IOError");
                    errorState = QAudio::IOError;

                } else if(l == 0) {
                    // cant write to IODevice
                    qWarning("QAudioInput: IOError, can't write to QIODevice");
                    errorState = QAudio::IOError;

                } else {
                    totalTimeValue += l;
                    errorState = QAudio::NoError;
                    if (deviceState != QAudio::ActiveState) {
                        deviceState = QAudio::ActiveState;
                        emit stateChanged(deviceState);
                    }
                    resuming = false;
                }
            } else {
                l = qMin<qint64>(len, waveBlocks[header].dwBytesRecorded - waveBlockOffset);
                // push mode
                memcpy(p, waveBlocks[header].lpData + waveBlockOffset, l);

                len -= l;

#ifdef DEBUG_AUDIO
                qDebug()<<"IN: "<<waveBlocks[header].dwBytesRecorded<<", OUT: "<<l;
#endif
                totalTimeValue += l;
                errorState = QAudio::NoError;
                if (deviceState != QAudio::ActiveState) {
                    deviceState = QAudio::ActiveState;
                    emit stateChanged(deviceState);
                }
                resuming = false;
            }
        } else {
            //no data, not ready yet, next time
            break;
        }

        if (l < waveBlocks[header].dwBytesRecorded - waveBlockOffset) {
            waveBlockOffset += l;
            done = true;
        } else {
            waveBlockOffset = 0;

            waveInUnprepareHeader(hWaveIn,&waveBlocks[header], sizeof(WAVEHDR));

            mutex.lock();
            waveFreeBlockCount++;
            mutex.unlock();

            waveBlocks[header].dwBytesRecorded=0;
            waveBlocks[header].dwFlags = 0L;
            result = waveInPrepareHeader(hWaveIn,&waveBlocks[header], sizeof(WAVEHDR));
            if(result != MMSYSERR_NOERROR) {
                result = waveInPrepareHeader(hWaveIn,&waveBlocks[header], sizeof(WAVEHDR));
                qWarning("QAudioInput: failed to prepare block %d,err=%d",header,result);
                errorState = QAudio::IOError;

                mutex.lock();
                waveFreeBlockCount--;
                mutex.unlock();

                return 0;
            }
            result = waveInAddBuffer(hWaveIn, &waveBlocks[header], sizeof(WAVEHDR));
            if(result != MMSYSERR_NOERROR) {
                qWarning("QAudioInput: failed to setup block %d,err=%d",header,result);
                errorState = QAudio::IOError;

                mutex.lock();
                waveFreeBlockCount--;
                mutex.unlock();

                return 0;
            }
            header++;
            if(header >= buffer_size/period_size)
                header = 0;
            p+=l;

            mutex.lock();
            if(!pullMode) {
                if(len < period_size || waveFreeBlockCount == buffer_size/period_size)
                    done = true;
            } else {
                if(waveFreeBlockCount == buffer_size/period_size)
                    done = true;
            }
            mutex.unlock();
        }

        written+=l;
    }
#ifdef DEBUG_AUDIO
    qDebug()<<"read in len="<<written;
#endif
    return written;
}

void QWindowsAudioInput::resume()
{
    if(deviceState == QAudio::SuspendedState) {
        deviceState = QAudio::ActiveState;
        for(int i=0; i<buffer_size/period_size; i++) {
            result = waveInAddBuffer(hWaveIn, &waveBlocks[i], sizeof(WAVEHDR));
            if(result != MMSYSERR_NOERROR) {
                qWarning("QAudioInput: failed to setup block %d,err=%d",i,result);
                errorState = QAudio::OpenError;
                deviceState = QAudio::StoppedState;
                emit stateChanged(deviceState);
                return;
            }
        }

        mutex.lock();
        waveFreeBlockCount = buffer_size/period_size;
        mutex.unlock();

        header = 0;
        resuming = true;
        waveBlockOffset = 0;
        waveInStart(hWaveIn);
        QTimer::singleShot(20,this,SLOT(feedback()));
        emit stateChanged(deviceState);
    }
}

void QWindowsAudioInput::setBufferSize(int value)
{
    buffer_size = value;
}

int QWindowsAudioInput::bufferSize() const
{
    return buffer_size;
}

int QWindowsAudioInput::periodSize() const
{
    return period_size;
}

void QWindowsAudioInput::setNotifyInterval(int ms)
{
    intervalTime = qMax(0, ms);
}

int QWindowsAudioInput::notifyInterval() const
{
    return intervalTime;
}

qint64 QWindowsAudioInput::processedUSecs() const
{
    if (deviceState == QAudio::StoppedState)
        return 0;
    qint64 result = qint64(1000000) * totalTimeValue /
        (settings.channelCount()*(settings.sampleSize()/8)) /
        settings.sampleRate();

    return result;
}

void QWindowsAudioInput::suspend()
{
    if(deviceState == QAudio::ActiveState) {
        waveInReset(hWaveIn);
        deviceState = QAudio::SuspendedState;
        emit stateChanged(deviceState);
    }
}

void QWindowsAudioInput::feedback()
{
#ifdef DEBUG_AUDIO
    QTime now(QTime::currentTime());
    qDebug()<<now.second()<<"s "<<now.msec()<<"ms :feedback() INPUT "<<this;
#endif
    if(!(deviceState==QAudio::StoppedState||deviceState==QAudio::SuspendedState))
        QMetaObject::invokeMethod(this, "deviceReady", Qt::QueuedConnection);
}

bool QWindowsAudioInput::deviceReady()
{
    bytesAvailable = bytesReady();
#ifdef DEBUG_AUDIO
    QTime now(QTime::currentTime());
    qDebug()<<now.second()<<"s "<<now.msec()<<"ms :deviceReady() INPUT";
#endif
    if(deviceState != QAudio::ActiveState && deviceState != QAudio::IdleState)
        return true;

    if(pullMode) {
        // reads some audio data and writes it to QIODevice
        read(0, buffer_size);
    } else {
        // emits readyRead() so user will call read() on QIODevice to get some audio data
        InputPrivate* a = qobject_cast<InputPrivate*>(audioSource);
        a->trigger();
    }

    if(intervalTime && (timeStamp.elapsed() + elapsedTimeOffset) > intervalTime) {
        emit notify();
        elapsedTimeOffset = timeStamp.elapsed() + elapsedTimeOffset - intervalTime;
        timeStamp.restart();
    }
    return true;
}

qint64 QWindowsAudioInput::elapsedUSecs() const
{
    if (deviceState == QAudio::StoppedState)
        return 0;

    return timeStampOpened.elapsed() * qint64(1000);
}

void QWindowsAudioInput::reset()
{
    stop();
    if (period_size > 0)
        waveFreeBlockCount = buffer_size / period_size;
}

InputPrivate::InputPrivate(QWindowsAudioInput* audio)
{
    audioDevice = qobject_cast<QWindowsAudioInput*>(audio);
}

InputPrivate::~InputPrivate() {}

qint64 InputPrivate::readData( char* data, qint64 len)
{
    // push mode, user read() called
    if(audioDevice->deviceState != QAudio::ActiveState &&
            audioDevice->deviceState != QAudio::IdleState)
        return 0;
    // Read in some audio data
    return audioDevice->read(data,len);
}

qint64 InputPrivate::writeData(const char* data, qint64 len)
{
    Q_UNUSED(data)
    Q_UNUSED(len)

    emit readyRead();
    return 0;
}

void InputPrivate::trigger()
{
    emit readyRead();
}

QT_END_NAMESPACE

#include "moc_qwindowsaudioinput.cpp"
