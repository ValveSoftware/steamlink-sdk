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

#include "v4lradiocontrol.h"
#include "v4lradioservice.h"

#include <QtCore/qdebug.h>

#include <fcntl.h>

#include <sys/ioctl.h>
#include "linux/videodev2.h"

#include <sys/soundcard.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

V4LRadioControl::V4LRadioControl(QObject *parent)
    :QRadioTunerControl(parent)
{
    fd = -1;
    initRadio();
    muted = false;
    stereo = false;
    m_error = false;
    sig = 0;
    currentBand = QRadioTuner::FM;
    step = 100000;
    scanning = false;
    playTime.restart();
    timer = new QTimer(this);
    timer->setInterval(200);
    connect(timer,SIGNAL(timeout()),this,SLOT(search()));
    timer->start();
}

V4LRadioControl::~V4LRadioControl()
{
    timer->stop();

    if(fd > 0)
        ::close(fd);
}

bool V4LRadioControl::isAvailable() const
{
    return available;
}

QMultimedia::AvailabilityStatus V4LRadioControl::availability() const
{
    if (fd > 0)
        return QMultimedia::Available;
    else
        return QMultimedia::ResourceError;
}

QRadioTuner::State V4LRadioControl::state() const
{
    return fd > 0 ? QRadioTuner::ActiveState : QRadioTuner::StoppedState;
}

QRadioTuner::Band V4LRadioControl::band() const
{
    return currentBand;
}

bool V4LRadioControl::isBandSupported(QRadioTuner::Band b) const
{
    QRadioTuner::Band bnd = (QRadioTuner::Band)b;
    switch(bnd) {
        case QRadioTuner::FM:
            if(freqMin <= 87500000 && freqMax >= 108000000)
                return true;
            break;
        case QRadioTuner::LW:
            if(freqMin <= 148500 && freqMax >= 283500)
                return true;
        case QRadioTuner::AM:
            if(freqMin <= 520000 && freqMax >= 1610000)
                return true;
        default:
            if(freqMin <= 1711000 && freqMax >= 30000000)
                return true;
    }

    return false;
}

void V4LRadioControl::setBand(QRadioTuner::Band b)
{
    if(freqMin <= 87500000 && freqMax >= 108000000 && b == QRadioTuner::FM) {
        // FM 87.5 to 108.0 MHz, except Japan 76-90 MHz
        currentBand =  (QRadioTuner::Band)b;
        step = 100000; // 100kHz steps
        emit bandChanged(currentBand);

    } else if(freqMin <= 148500 && freqMax >= 283500 && b == QRadioTuner::LW) {
        // LW 148.5 to 283.5 kHz, 9kHz channel spacing (Europe, Africa, Asia)
        currentBand =  (QRadioTuner::Band)b;
        step = 1000; // 1kHz steps
        emit bandChanged(currentBand);

    } else if(freqMin <= 520000 && freqMax >= 1610000 && b == QRadioTuner::AM) {
        // AM 520 to 1610 kHz, 9 or 10kHz channel spacing, extended 1610 to 1710 kHz
        currentBand =  (QRadioTuner::Band)b;
        step = 1000; // 1kHz steps
        emit bandChanged(currentBand);

    } else if(freqMin <= 1711000 && freqMax >= 30000000 && b == QRadioTuner::SW) {
        // SW 1.711 to 30.0 MHz, divided into 15 bands. 5kHz channel spacing
        currentBand =  (QRadioTuner::Band)b;
        step = 500; // 500Hz steps
        emit bandChanged(currentBand);
    }
    playTime.restart();
}

int V4LRadioControl::frequency() const
{
    return currentFreq;
}

int V4LRadioControl::frequencyStep(QRadioTuner::Band b) const
{
    int step = 0;

    if(b == QRadioTuner::FM)
        step = 100000; // 100kHz steps
    else if(b == QRadioTuner::LW)
        step = 1000; // 1kHz steps
    else if(b == QRadioTuner::AM)
        step = 1000; // 1kHz steps
    else if(b == QRadioTuner::SW)
        step = 500; // 500Hz steps

    return step;
}

QPair<int,int> V4LRadioControl::frequencyRange(QRadioTuner::Band b) const
{
    if(b == QRadioTuner::AM)
        return qMakePair<int,int>(520000,1710000);
    else if(b == QRadioTuner::FM)
        return qMakePair<int,int>(87500000,108000000);
    else if(b == QRadioTuner::SW)
        return qMakePair<int,int>(1711111,30000000);
    else if(b == QRadioTuner::LW)
        return qMakePair<int,int>(148500,283500);

    return qMakePair<int,int>(0,0);
}

void V4LRadioControl::setFrequency(int frequency)
{
    qint64 f = frequency;

    v4l2_frequency freq;

    if(frequency < freqMin)
        f = freqMax;
    if(frequency > freqMax)
        f = freqMin;

    if(fd > 0) {
        memset( &freq, 0, sizeof( freq ) );
        // Use the first tuner
        freq.tuner = 0;
        if ( ioctl( fd, VIDIOC_G_FREQUENCY, &freq ) >= 0 ) {
            if(low) {
                // For low, freq in units of 62.5Hz, so convert from Hz to units.
                freq.frequency = (int)(f/62.5);
            } else {
                // For high, freq in units of 62.5kHz, so convert from Hz to units.
                freq.frequency = (int)(f/62500);
            }
            ioctl( fd, VIDIOC_S_FREQUENCY, &freq );
            currentFreq = f;
            playTime.restart();
            emit frequencyChanged(currentFreq);
        }
    }
    playTime.restart();
}

bool V4LRadioControl::isStereo() const
{
    return stereo;
}

QRadioTuner::StereoMode V4LRadioControl::stereoMode() const
{
    return QRadioTuner::Auto;
}

void V4LRadioControl::setStereoMode(QRadioTuner::StereoMode mode)
{
    bool stereo = true;

    if(mode == QRadioTuner::ForceMono)
        stereo = false;

    v4l2_tuner tuner;

    memset( &tuner, 0, sizeof( tuner ) );

    if ( ioctl( fd, VIDIOC_G_TUNER, &tuner ) >= 0 ) {
        if(stereo)
            tuner.audmode = V4L2_TUNER_MODE_STEREO;
        else
            tuner.audmode = V4L2_TUNER_MODE_MONO;

        if ( ioctl( fd, VIDIOC_S_TUNER, &tuner ) >= 0 ) {
            emit stereoStatusChanged(stereo);
        }
    }
}

int V4LRadioControl::signalStrength() const
{
    v4l2_tuner tuner;

    // Return the first tuner founds signal strength.
    for ( int index = 0; index < tuners; ++index ) {
        memset( &tuner, 0, sizeof( tuner ) );
        tuner.index = index;
        if ( ioctl( fd, VIDIOC_G_TUNER, &tuner ) < 0 )
            continue;
        if ( tuner.type != V4L2_TUNER_RADIO )
            continue;
        // percentage signal strength
        return tuner.signal*100/65535;
    }

    return 0;
}

int V4LRadioControl::volume() const
{
    v4l2_queryctrl queryctrl;

    if(fd > 0) {
        memset( &queryctrl, 0, sizeof( queryctrl ) );
        queryctrl.id = V4L2_CID_AUDIO_VOLUME;
        if ( ioctl( fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 ) {
            if(queryctrl.maximum == 0) {
                return vol;
            } else {
                // percentage volume returned
                return queryctrl.default_value*100/queryctrl.maximum;
            }
        }
    }
    return 0;
}

void V4LRadioControl::setVolume(int volume)
{
    v4l2_queryctrl queryctrl;

    if(fd > 0) {
        memset( &queryctrl, 0, sizeof( queryctrl ) );
        queryctrl.id = V4L2_CID_AUDIO_VOLUME;
        if ( ioctl( fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 ) {
            v4l2_control control;

            if(queryctrl.maximum > 0) {
                memset( &control, 0, sizeof( control ) );
                control.id = V4L2_CID_AUDIO_VOLUME;
                control.value = volume*queryctrl.maximum/100;
                ioctl( fd, VIDIOC_S_CTRL, &control );
            } else {
                setVol(volume);
            }
            emit volumeChanged(volume);
        }
    }
}

bool V4LRadioControl::isMuted() const
{
    return muted;
}

void V4LRadioControl::setMuted(bool muted)
{
    v4l2_queryctrl queryctrl;

    if(fd > 0) {
        memset( &queryctrl, 0, sizeof( queryctrl ) );
        queryctrl.id = V4L2_CID_AUDIO_MUTE;
        if ( ioctl( fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 ) {
            v4l2_control control;
            memset( &control, 0, sizeof( control ) );
            control.id = V4L2_CID_AUDIO_MUTE;
            control.value = (muted ? queryctrl.maximum : queryctrl.minimum );
            ioctl( fd, VIDIOC_S_CTRL, &control );
            this->muted = muted;
            emit mutedChanged(muted);
        }
    }
}

bool V4LRadioControl::isSearching() const
{
    return scanning;
}

void V4LRadioControl::cancelSearch()
{
    scanning = false;
    timer->stop();
}

void V4LRadioControl::searchForward()
{
    // Scan up
    if(scanning) {
        cancelSearch();
        return;
    }
    scanning = true;
    forward  = true;
    timer->start();
}

void V4LRadioControl::searchBackward()
{
    // Scan down
    if(scanning) {
        cancelSearch();
        return;
    }
    scanning = true;
    forward  = false;
    timer->start();
}

void V4LRadioControl::start()
{
}

void V4LRadioControl::stop()
{
}

QRadioTuner::Error V4LRadioControl::error() const
{
    if(m_error)
        return QRadioTuner::OpenError;

    return QRadioTuner::NoError;
}

QString V4LRadioControl::errorString() const
{
    return QString();
}

void V4LRadioControl::search()
{
    int signal = signalStrength();
    if(sig != signal) {
        sig = signal;
        emit signalStrengthChanged(sig);
    }

    if(!scanning) return;

    if (signal > 25) {
        cancelSearch();
        return;
    }

    if(forward) {
        setFrequency(currentFreq+step);
    } else {
        setFrequency(currentFreq-step);
    }
}

bool V4LRadioControl::initRadio()
{
    v4l2_tuner tuner;
    v4l2_input input;
    v4l2_frequency freq;
    v4l2_capability cap;

    low = false;
    available = false;
    freqMin = freqMax = currentFreq = 0;

    fd = ::open("/dev/radio0", O_RDWR);

    if(fd != -1) {
        // Capabilities
        memset( &cap, 0, sizeof( cap ) );
        if(::ioctl(fd, VIDIOC_QUERYCAP, &cap ) >= 0) {
            if(((cap.capabilities & V4L2_CAP_RADIO) == 0) && ((cap.capabilities & V4L2_CAP_AUDIO) == 0))
                available = true;
        }

        // Tuners
        memset( &input, 0, sizeof( input ) );
        tuners = 0;
        for ( ;; ) {
            memset( &input, 0, sizeof( input ) );
            input.index = tuners;
            if ( ioctl( fd, VIDIOC_ENUMINPUT, &input ) < 0 )
                break;
            ++tuners;
        }

        // Freq bands
        for ( int index = 0; index < tuners; ++index ) {
            memset( &tuner, 0, sizeof( tuner ) );
            tuner.index = index;
            if ( ioctl( fd, VIDIOC_G_TUNER, &tuner ) < 0 )
                continue;
            if ( tuner.type != V4L2_TUNER_RADIO )
                continue;
            if ( ( tuner.capability & V4L2_TUNER_CAP_LOW ) != 0 ) {
                // Units are 1/16th of a kHz.
                low = true;
            }
            if(low) {
                freqMin = tuner.rangelow * 62.5;
                freqMax = tuner.rangehigh * 62.5;
            } else {
                freqMin = tuner.rangelow * 62500;
                freqMax = tuner.rangehigh * 62500;
            }
        }

        // frequency
        memset( &freq, 0, sizeof( freq ) );
        if(::ioctl(fd, VIDIOC_G_FREQUENCY, &freq ) >= 0) {
            if ( ((int)freq.frequency) != -1 ) {    // -1 means not set.
                if(low)
                    currentFreq = freq.frequency * 62.5;
                else
                    currentFreq = freq.frequency * 62500;
            }
        }

        // stereo
        bool stereo = false;
        memset( &tuner, 0, sizeof( tuner ) );
        if ( ioctl( fd, VIDIOC_G_TUNER, &tuner ) >= 0 ) {
            if((tuner.rxsubchans & V4L2_TUNER_SUB_STEREO) != 0)
                stereo = true;
        }

        vol = getVol();

        return true;
    }
    m_error = true;
    emit error();

    return false;
}

void V4LRadioControl::setVol(int v)
{
    int fd = ::open( "/dev/mixer", O_RDWR, 0 );
    if ( fd < 0 )
        return;
    int volume = v;
    if ( volume < 0 )
        volume = 0;
    else if ( volume > 100 )
        volume = 100;
    vol = volume;
    volume += volume << 8;
    ::ioctl( fd, MIXER_WRITE(SOUND_MIXER_VOLUME), &volume );
    ::close( fd );
}

int V4LRadioControl::getVol()
{
    int fd = ::open( "/dev/mixer", O_RDWR, 0 );
    if ( fd >= 0 ) {
        int volume = 0;
        ::ioctl( fd, MIXER_READ(SOUND_MIXER_VOLUME), &volume );
        int left = ( volume & 0xFF );
        int right = ( ( volume >> 8 ) & 0xFF );
        if ( left > right )
            vol = left;
        else
            vol = right;
        ::close( fd );
        return vol;
    }
    return 0;
}

