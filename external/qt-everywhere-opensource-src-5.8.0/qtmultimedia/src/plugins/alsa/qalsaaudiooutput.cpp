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

#include <QtCore/qcoreapplication.h>
#include <QtCore/qvarlengtharray.h>
#include <QtMultimedia/private/qaudiohelpers_p.h>
#include "qalsaaudiooutput.h"
#include "qalsaaudiodeviceinfo.h"

QT_BEGIN_NAMESPACE

//#define DEBUG_AUDIO 1

QAlsaAudioOutput::QAlsaAudioOutput(const QByteArray &device)
{
    bytesAvailable = 0;
    handle = 0;
    access = SND_PCM_ACCESS_RW_INTERLEAVED;
    pcmformat = SND_PCM_FORMAT_S16;
    buffer_frames = 0;
    period_frames = 0;
    buffer_size = 0;
    period_size = 0;
    buffer_time = 100000;
    period_time = 20000;
    totalTimeValue = 0;
    intervalTime = 1000;
    audioBuffer = 0;
    errorState = QAudio::NoError;
    deviceState = QAudio::StoppedState;
    audioSource = 0;
    pullMode = true;
    resuming = false;
    opened = false;

    m_volume = 1.0f;

    m_device = device;

    timer = new QTimer(this);
    connect(timer,SIGNAL(timeout()),SLOT(userFeed()));
}

QAlsaAudioOutput::~QAlsaAudioOutput()
{
    close();
    disconnect(timer, SIGNAL(timeout()));
    QCoreApplication::processEvents();
    delete timer;
}

void QAlsaAudioOutput::setVolume(qreal vol)
{
    m_volume = vol;
}

qreal QAlsaAudioOutput::volume() const
{
    return m_volume;
}

QAudio::Error QAlsaAudioOutput::error() const
{
    return errorState;
}

QAudio::State QAlsaAudioOutput::state() const
{
    return deviceState;
}

int QAlsaAudioOutput::xrun_recovery(int err)
{
    int  count = 0;
    bool reset = false;

    // ESTRPIPE is not available in all OSes where ALSA is available
    int estrpipe = EIO;
#ifdef ESTRPIPE
    estrpipe = ESTRPIPE;
#endif

    if(err == -EPIPE) {
        errorState = QAudio::UnderrunError;
        emit errorChanged(errorState);
        err = snd_pcm_prepare(handle);
        if(err < 0)
            reset = true;

    } else if ((err == -estrpipe)||(err == -EIO)) {
        errorState = QAudio::IOError;
        emit errorChanged(errorState);
        while((err = snd_pcm_resume(handle)) == -EAGAIN){
            usleep(100);
            count++;
            if(count > 5) {
                reset = true;
                break;
            }
        }
        if(err < 0) {
            err = snd_pcm_prepare(handle);
            if(err < 0)
                reset = true;
        }
    }
    if(reset) {
        close();
        open();
        snd_pcm_prepare(handle);
        return 0;
    }
    return err;
}

int QAlsaAudioOutput::setFormat()
{
    snd_pcm_format_t pcmformat = SND_PCM_FORMAT_UNKNOWN;

    if(settings.sampleSize() == 8) {
        pcmformat = SND_PCM_FORMAT_U8;

    } else if(settings.sampleSize() == 16) {
        if(settings.sampleType() == QAudioFormat::SignedInt) {
            if(settings.byteOrder() == QAudioFormat::LittleEndian)
                pcmformat = SND_PCM_FORMAT_S16_LE;
            else
                pcmformat = SND_PCM_FORMAT_S16_BE;
        } else if(settings.sampleType() == QAudioFormat::UnSignedInt) {
            if(settings.byteOrder() == QAudioFormat::LittleEndian)
                pcmformat = SND_PCM_FORMAT_U16_LE;
            else
                pcmformat = SND_PCM_FORMAT_U16_BE;
        }
    } else if(settings.sampleSize() == 24) {
        if(settings.sampleType() == QAudioFormat::SignedInt) {
            if(settings.byteOrder() == QAudioFormat::LittleEndian)
                pcmformat = SND_PCM_FORMAT_S24_LE;
            else
                pcmformat = SND_PCM_FORMAT_S24_BE;
        } else if(settings.sampleType() == QAudioFormat::UnSignedInt) {
            if(settings.byteOrder() == QAudioFormat::LittleEndian)
                pcmformat = SND_PCM_FORMAT_U24_LE;
            else
                pcmformat = SND_PCM_FORMAT_U24_BE;
        }
    } else if(settings.sampleSize() == 32) {
        if(settings.sampleType() == QAudioFormat::SignedInt) {
            if(settings.byteOrder() == QAudioFormat::LittleEndian)
                pcmformat = SND_PCM_FORMAT_S32_LE;
            else
                pcmformat = SND_PCM_FORMAT_S32_BE;
        } else if(settings.sampleType() == QAudioFormat::UnSignedInt) {
            if(settings.byteOrder() == QAudioFormat::LittleEndian)
                pcmformat = SND_PCM_FORMAT_U32_LE;
            else
                pcmformat = SND_PCM_FORMAT_U32_BE;
        } else if(settings.sampleType() == QAudioFormat::Float) {
            if(settings.byteOrder() == QAudioFormat::LittleEndian)
                pcmformat = SND_PCM_FORMAT_FLOAT_LE;
            else
                pcmformat = SND_PCM_FORMAT_FLOAT_BE;
        }
    } else if(settings.sampleSize() == 64) {
        if(settings.byteOrder() == QAudioFormat::LittleEndian)
            pcmformat = SND_PCM_FORMAT_FLOAT64_LE;
        else
            pcmformat = SND_PCM_FORMAT_FLOAT64_BE;
    }

    return pcmformat != SND_PCM_FORMAT_UNKNOWN
            ? snd_pcm_hw_params_set_format( handle, hwparams, pcmformat)
            : -1;
}

void QAlsaAudioOutput::start(QIODevice* device)
{
    if(deviceState != QAudio::StoppedState)
        deviceState = QAudio::StoppedState;

    errorState = QAudio::NoError;

    // Handle change of mode
    if(audioSource && !pullMode) {
        delete audioSource;
        audioSource = 0;
    }

    close();

    pullMode = true;
    audioSource = device;

    deviceState = QAudio::ActiveState;

    open();

    emit stateChanged(deviceState);
}

QIODevice* QAlsaAudioOutput::start()
{
    if(deviceState != QAudio::StoppedState)
        deviceState = QAudio::StoppedState;

    errorState = QAudio::NoError;

    // Handle change of mode
    if(audioSource && !pullMode) {
        delete audioSource;
        audioSource = 0;
    }

    close();

    audioSource = new AlsaOutputPrivate(this);
    audioSource->open(QIODevice::WriteOnly|QIODevice::Unbuffered);
    pullMode = false;

    deviceState = QAudio::IdleState;

    open();

    emit stateChanged(deviceState);

    return audioSource;
}

void QAlsaAudioOutput::stop()
{
    if(deviceState == QAudio::StoppedState)
        return;
    errorState = QAudio::NoError;
    deviceState = QAudio::StoppedState;
    close();
    emit stateChanged(deviceState);
}

bool QAlsaAudioOutput::open()
{
    if(opened)
        return true;

#ifdef DEBUG_AUDIO
    QTime now(QTime::currentTime());
    qDebug()<<now.second()<<"s "<<now.msec()<<"ms :open()";
#endif
    timeStamp.restart();
    elapsedTimeOffset = 0;

    int dir;
    int err = 0;
    int count=0;
    unsigned int sampleRate=settings.sampleRate();

    if (!settings.isValid()) {
        qWarning("QAudioOutput: open error, invalid format.");
    } else if (settings.sampleRate() <= 0) {
        qWarning("QAudioOutput: open error, invalid sample rate (%d).",
                 settings.sampleRate());
    } else {
        err = -1;
    }

    if (err == 0) {
        errorState = QAudio::OpenError;
        deviceState = QAudio::StoppedState;
        emit errorChanged(errorState);
        return false;
    }

    if (!QAlsaAudioDeviceInfo::availableDevices(QAudio::AudioOutput).contains(m_device))
        return false;

    QString dev;
#if SND_LIB_VERSION < 0x1000e  // 1.0.14
    if (m_device != "default")
        dev = QAlsaAudioDeviceInfo::deviceFromCardName(m_device);
    else
#endif
        dev = m_device;

    // Step 1: try and open the device
    while((count < 5) && (err < 0)) {
        err=snd_pcm_open(&handle,dev.toLocal8Bit().constData(),SND_PCM_STREAM_PLAYBACK,0);
        if(err < 0)
            count++;
    }
    if (( err < 0)||(handle == 0)) {
        errorState = QAudio::OpenError;
        emit errorChanged(errorState);
        deviceState = QAudio::StoppedState;
        return false;
    }
    snd_pcm_nonblock( handle, 0 );

    // Step 2: Set the desired HW parameters.
    snd_pcm_hw_params_alloca( &hwparams );

    bool fatal = false;
    QString errMessage;
    unsigned int chunks = 8;

    err = snd_pcm_hw_params_any( handle, hwparams );
    if ( err < 0 ) {
        fatal = true;
        errMessage = QString::fromLatin1("QAudioOutput: snd_pcm_hw_params_any: err = %1").arg(err);
    }
    if ( !fatal ) {
        err = snd_pcm_hw_params_set_rate_resample( handle, hwparams, 1 );
        if ( err < 0 ) {
            fatal = true;
            errMessage = QString::fromLatin1("QAudioOutput: snd_pcm_hw_params_set_rate_resample: err = %1").arg(err);
        }
    }
    if ( !fatal ) {
        err = snd_pcm_hw_params_set_access( handle, hwparams, access );
        if ( err < 0 ) {
            fatal = true;
            errMessage = QString::fromLatin1("QAudioOutput: snd_pcm_hw_params_set_access: err = %1").arg(err);
        }
    }
    if ( !fatal ) {
        err = setFormat();
        if ( err < 0 ) {
            fatal = true;
            errMessage = QString::fromLatin1("QAudioOutput: snd_pcm_hw_params_set_format: err = %1").arg(err);
        }
    }
    if ( !fatal ) {
        err = snd_pcm_hw_params_set_channels( handle, hwparams, (unsigned int)settings.channelCount() );
        if ( err < 0 ) {
            fatal = true;
            errMessage = QString::fromLatin1("QAudioOutput: snd_pcm_hw_params_set_channels: err = %1").arg(err);
        }
    }
    if ( !fatal ) {
        err = snd_pcm_hw_params_set_rate_near( handle, hwparams, &sampleRate, 0 );
        if ( err < 0 ) {
            fatal = true;
            errMessage = QString::fromLatin1("QAudioOutput: snd_pcm_hw_params_set_rate_near: err = %1").arg(err);
        }
    }
    if ( !fatal ) {
        unsigned int maxBufferTime = 0;
        unsigned int minBufferTime = 0;
        unsigned int maxPeriodTime = 0;
        unsigned int minPeriodTime = 0;

        err = snd_pcm_hw_params_get_buffer_time_max(hwparams, &maxBufferTime, &dir);
        if ( err >= 0)
            err = snd_pcm_hw_params_get_buffer_time_min(hwparams, &minBufferTime, &dir);
        if ( err >= 0)
            err = snd_pcm_hw_params_get_period_time_max(hwparams, &maxPeriodTime, &dir);
        if ( err >= 0)
            err = snd_pcm_hw_params_get_period_time_min(hwparams, &minPeriodTime, &dir);

        if ( err < 0 ) {
            fatal = true;
            errMessage = QString::fromLatin1("QAudioOutput: buffer/period min and max: err = %1").arg(err);
        } else {
            if (maxBufferTime < buffer_time || buffer_time < minBufferTime || maxPeriodTime < period_time || minPeriodTime > period_time) {
#ifdef DEBUG_AUDIO
                qDebug()<<"defaults out of range";
                qDebug()<<"pmin="<<minPeriodTime<<", pmax="<<maxPeriodTime<<", bmin="<<minBufferTime<<", bmax="<<maxBufferTime;
#endif
                period_time = minPeriodTime;
                if (period_time*4 <= maxBufferTime) {
                    // Use 4 periods if possible
                    buffer_time = period_time*4;
                    chunks = 4;
                } else if (period_time*2 <= maxBufferTime) {
                    // Use 2 periods if possible
                    buffer_time = period_time*2;
                    chunks = 2;
                } else {
                    qWarning()<<"QAudioOutput: alsa only supports single period!";
                    fatal = true;
                }
#ifdef DEBUG_AUDIO
                qDebug()<<"used: buffer_time="<<buffer_time<<", period_time="<<period_time;
#endif
            }
        }
    }
    if ( !fatal ) {
        err = snd_pcm_hw_params_set_buffer_time_near(handle, hwparams, &buffer_time, &dir);
        if ( err < 0 ) {
            fatal = true;
            errMessage = QString::fromLatin1("QAudioOutput: snd_pcm_hw_params_set_buffer_time_near: err = %1").arg(err);
        }
    }
    if ( !fatal ) {
        err = snd_pcm_hw_params_set_period_time_near(handle, hwparams, &period_time, &dir);
        if ( err < 0 ) {
            fatal = true;
            errMessage = QString::fromLatin1("QAudioOutput: snd_pcm_hw_params_set_period_time_near: err = %1").arg(err);
        }
    }
    if ( !fatal ) {
        err = snd_pcm_hw_params_set_periods_near(handle, hwparams, &chunks, &dir);
        if ( err < 0 ) {
            fatal = true;
            errMessage = QString::fromLatin1("QAudioOutput: snd_pcm_hw_params_set_periods_near: err = %1").arg(err);
        }
    }
    if ( !fatal ) {
        err = snd_pcm_hw_params(handle, hwparams);
        if ( err < 0 ) {
            fatal = true;
            errMessage = QString::fromLatin1("QAudioOutput: snd_pcm_hw_params: err = %1").arg(err);
        }
    }
    if( err < 0) {
        qWarning()<<errMessage;
        errorState = QAudio::OpenError;
        emit errorChanged(errorState);
        deviceState = QAudio::StoppedState;
        return false;
    }
    snd_pcm_hw_params_get_buffer_size(hwparams,&buffer_frames);
    buffer_size = snd_pcm_frames_to_bytes(handle,buffer_frames);
    snd_pcm_hw_params_get_period_size(hwparams,&period_frames, &dir);
    period_size = snd_pcm_frames_to_bytes(handle,period_frames);
    snd_pcm_hw_params_get_buffer_time(hwparams,&buffer_time, &dir);
    snd_pcm_hw_params_get_period_time(hwparams,&period_time, &dir);

    // Step 3: Set the desired SW parameters.
    snd_pcm_sw_params_t *swparams;
    snd_pcm_sw_params_alloca(&swparams);
    snd_pcm_sw_params_current(handle, swparams);
    snd_pcm_sw_params_set_start_threshold(handle,swparams,period_frames);
    snd_pcm_sw_params_set_stop_threshold(handle,swparams,buffer_frames);
    snd_pcm_sw_params_set_avail_min(handle, swparams,period_frames);
    snd_pcm_sw_params(handle, swparams);

    // Step 4: Prepare audio
    if(audioBuffer == 0)
        audioBuffer = new char[snd_pcm_frames_to_bytes(handle,buffer_frames)];
    snd_pcm_prepare( handle );
    snd_pcm_start(handle);

    // Step 5: Setup timer
    bytesAvailable = bytesFree();

    // Step 6: Start audio processing
    timer->start(period_time/1000);

    clockStamp.restart();
    timeStamp.restart();
    elapsedTimeOffset = 0;
    errorState  = QAudio::NoError;
    totalTimeValue = 0;
    opened = true;

    return true;
}

void QAlsaAudioOutput::close()
{
    timer->stop();

    if ( handle ) {
        snd_pcm_drain( handle );
        snd_pcm_close( handle );
        handle = 0;
        delete [] audioBuffer;
        audioBuffer=0;
    }
    if(!pullMode && audioSource) {
        delete audioSource;
        audioSource = 0;
    }
    opened = false;
}

int QAlsaAudioOutput::bytesFree() const
{
    if(resuming)
        return period_size;

    if(deviceState != QAudio::ActiveState && deviceState != QAudio::IdleState)
        return 0;

    int frames = snd_pcm_avail_update(handle);
    if (frames == -EPIPE) {
        // Try and handle buffer underrun
        int err = snd_pcm_recover(handle, frames, 0);
        if (err < 0)
            return 0;
        else
            frames = snd_pcm_avail_update(handle);
    } else if (frames < 0) {
        return 0;
    }

    if ((int)frames > (int)buffer_frames)
        frames = buffer_frames;

    return snd_pcm_frames_to_bytes(handle, frames);
}

qint64 QAlsaAudioOutput::write( const char *data, qint64 len )
{
    // Write out some audio data
    if ( !handle )
        return 0;
#ifdef DEBUG_AUDIO
    qDebug()<<"frames to write out = "<<
        snd_pcm_bytes_to_frames( handle, (int)len )<<" ("<<len<<") bytes";
#endif
    int frames, err;
    int space = bytesFree();

    if (!space)
        return 0;

    if (len < space)
        space = len;

    frames = snd_pcm_bytes_to_frames(handle, space);

    if (m_volume < 1.0f) {
        QVarLengthArray<char, 4096> out(space);
        QAudioHelperInternal::qMultiplySamples(m_volume, settings, data, out.data(), space);
        err = snd_pcm_writei(handle, out.constData(), frames);
    } else {
        err = snd_pcm_writei(handle, data, frames);
    }

    if(err > 0) {
        totalTimeValue += err;
        resuming = false;
        errorState = QAudio::NoError;
        if (deviceState != QAudio::ActiveState) {
            deviceState = QAudio::ActiveState;
            emit stateChanged(deviceState);
        }
        return snd_pcm_frames_to_bytes( handle, err );
    } else
        err = xrun_recovery(err);

    if(err < 0) {
        close();
        errorState = QAudio::FatalError;
        emit errorChanged(errorState);
        deviceState = QAudio::StoppedState;
        emit stateChanged(deviceState);
    }
    return 0;
}

int QAlsaAudioOutput::periodSize() const
{
    return period_size;
}

void QAlsaAudioOutput::setBufferSize(int value)
{
    if(deviceState == QAudio::StoppedState)
        buffer_size = value;
}

int QAlsaAudioOutput::bufferSize() const
{
    return buffer_size;
}

void QAlsaAudioOutput::setNotifyInterval(int ms)
{
    intervalTime = qMax(0, ms);
}

int QAlsaAudioOutput::notifyInterval() const
{
    return intervalTime;
}

qint64 QAlsaAudioOutput::processedUSecs() const
{
    return qint64(1000000) * totalTimeValue / settings.sampleRate();
}

void QAlsaAudioOutput::resume()
{
    if(deviceState == QAudio::SuspendedState) {
        int err = 0;

        if(handle) {
            err = snd_pcm_prepare( handle );
            if(err < 0)
                xrun_recovery(err);

            err = snd_pcm_start(handle);
            if(err < 0)
                xrun_recovery(err);

            bytesAvailable = (int)snd_pcm_frames_to_bytes(handle, buffer_frames);
        }
        resuming = true;

        deviceState = pullMode ? QAudio::ActiveState : QAudio::IdleState;

        errorState = QAudio::NoError;
        timer->start(period_time/1000);
        emit stateChanged(deviceState);
    }
}

void QAlsaAudioOutput::setFormat(const QAudioFormat& fmt)
{
    if (deviceState == QAudio::StoppedState)
        settings = fmt;
}

QAudioFormat QAlsaAudioOutput::format() const
{
    return settings;
}

void QAlsaAudioOutput::suspend()
{
    if(deviceState == QAudio::ActiveState || deviceState == QAudio::IdleState || resuming) {
        snd_pcm_drain(handle);
        timer->stop();
        deviceState = QAudio::SuspendedState;
        errorState = QAudio::NoError;
        emit stateChanged(deviceState);
    }
}

void QAlsaAudioOutput::userFeed()
{
    if(deviceState == QAudio::StoppedState || deviceState == QAudio::SuspendedState)
        return;
#ifdef DEBUG_AUDIO
    QTime now(QTime::currentTime());
    qDebug()<<now.second()<<"s "<<now.msec()<<"ms :userFeed() OUT";
#endif
    if(deviceState ==  QAudio::IdleState)
        bytesAvailable = bytesFree();

    deviceReady();
}

bool QAlsaAudioOutput::deviceReady()
{
    if(pullMode) {
        int l = 0;
        int chunks = bytesAvailable/period_size;
        if(chunks==0) {
            bytesAvailable = bytesFree();
            return false;
        }
#ifdef DEBUG_AUDIO
        qDebug()<<"deviceReady() avail="<<bytesAvailable<<" bytes, period size="<<period_size<<" bytes";
        qDebug()<<"deviceReady() no. of chunks that can fit ="<<chunks<<", chunks in bytes ="<<period_size*chunks;
#endif
        int input = period_frames*chunks;
        if(input > (int)buffer_frames)
            input = buffer_frames;
        l = audioSource->read(audioBuffer,snd_pcm_frames_to_bytes(handle, input));

        // reading can take a while and stream may have been stopped
        if (!handle)
            return false;

        if(l > 0) {
            // Got some data to output
            if(deviceState != QAudio::ActiveState)
                return true;
            qint64 bytesWritten = write(audioBuffer,l);
            if (bytesWritten != l)
                audioSource->seek(audioSource->pos()-(l-bytesWritten));
            bytesAvailable = bytesFree();

        } else if(l == 0) {
            // Did not get any data to output
            bytesAvailable = bytesFree();
            if(bytesAvailable > snd_pcm_frames_to_bytes(handle, buffer_frames-period_frames)) {
                // Underrun
                if (deviceState != QAudio::IdleState) {
                    errorState = QAudio::UnderrunError;
                    emit errorChanged(errorState);
                    deviceState = QAudio::IdleState;
                    emit stateChanged(deviceState);
                }
            }

        } else if(l < 0) {
            close();
            deviceState = QAudio::StoppedState;
            errorState = QAudio::IOError;
            emit errorChanged(errorState);
            emit stateChanged(deviceState);
        }
    } else {
        bytesAvailable = bytesFree();
        if(bytesAvailable > snd_pcm_frames_to_bytes(handle, buffer_frames-period_frames)) {
            // Underrun
            if (deviceState != QAudio::IdleState) {
                errorState = QAudio::UnderrunError;
                emit errorChanged(errorState);
                deviceState = QAudio::IdleState;
                emit stateChanged(deviceState);
            }
        }
    }

    if(deviceState != QAudio::ActiveState)
        return true;

    if(intervalTime && (timeStamp.elapsed() + elapsedTimeOffset) > intervalTime) {
        emit notify();
        elapsedTimeOffset = timeStamp.elapsed() + elapsedTimeOffset - intervalTime;
        timeStamp.restart();
    }
    return true;
}

qint64 QAlsaAudioOutput::elapsedUSecs() const
{
    if (deviceState == QAudio::StoppedState)
        return 0;

    return clockStamp.elapsed() * qint64(1000);
}

void QAlsaAudioOutput::reset()
{
    if(handle)
        snd_pcm_reset(handle);

    stop();
}

AlsaOutputPrivate::AlsaOutputPrivate(QAlsaAudioOutput* audio)
{
    audioDevice = qobject_cast<QAlsaAudioOutput*>(audio);
}

AlsaOutputPrivate::~AlsaOutputPrivate() {}

qint64 AlsaOutputPrivate::readData( char* data, qint64 len)
{
    Q_UNUSED(data)
    Q_UNUSED(len)

    return 0;
}

qint64 AlsaOutputPrivate::writeData(const char* data, qint64 len)
{
    int retry = 0;
    qint64 written = 0;
    if((audioDevice->deviceState == QAudio::ActiveState)
            ||(audioDevice->deviceState == QAudio::IdleState)) {
        while(written < len) {
            int chunk = audioDevice->write(data+written,(len-written));
            if(chunk <= 0)
                retry++;
            written+=chunk;
            if(retry > 10)
                return written;
        }
    }
    return written;

}

QT_END_NAMESPACE

#include "moc_qalsaaudiooutput.cpp"
