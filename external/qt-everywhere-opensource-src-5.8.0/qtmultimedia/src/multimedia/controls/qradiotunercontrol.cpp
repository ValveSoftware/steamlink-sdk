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

#include <qtmultimediaglobal.h>
#include "qradiotunercontrol.h"
#include "qmediacontrol_p.h"

QT_BEGIN_NAMESPACE


/*!
    \class QRadioTunerControl
    \inmodule QtMultimedia


    \ingroup multimedia_control


    \brief The QRadioTunerControl class provides access to the radio tuning
    functionality of a QMediaService.

    If a QMediaService can tune an analog radio device it will implement
    QRadioTunerControl.  This control provides a means to tune a radio device
    to a specific \l {setFrequency()}{frequency} as well as search \l
    {searchForward()}{forwards} and \l {searchBackward()}{backwards} for a
    signal.

    The functionality provided by this control is exposed to application code
    through the QRadioTuner class.

    The interface name of QRadioTunerControl is \c org.qt-project.qt.radiotunercontrol/5.0 as
    defined in QRadioTunerControl_iid.

    \sa QMediaService::requestControl(), QRadioTuner
*/

/*!
    \macro QRadioTunerControl_iid

    \c org.qt-project.qt.radiotunercontrol/5.0

    Defines the interface name of the QRadioTunerControl class.

    \relates QRadioTunerControl
*/

/*!
    Constructs a radio tuner control with the given \a parent.
*/

QRadioTunerControl::QRadioTunerControl(QObject *parent):
    QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
    Destroys a radio tuner control.
*/

QRadioTunerControl::~QRadioTunerControl()
{
}

/*!
    \fn QRadioTuner::State QRadioTunerControl::state() const

    Returns the current radio tuner state.
*/

/*!
    \fn QRadioTuner::Band QRadioTunerControl::band() const

    Returns the frequency band a radio tuner is tuned to.
*/

/*!
    \fn void QRadioTunerControl::bandChanged(QRadioTuner::Band band)

    Signals that the frequency \a band a radio tuner is tuned to has changed.
*/

/*!
    \fn void QRadioTunerControl::setBand(QRadioTuner::Band band)

    Sets the frequecy \a band a radio tuner is tuned to.

    Changing the frequency band will reset the frequency to the minimum frequency of the new band.
*/

/*!
    \fn bool QRadioTunerControl::isBandSupported(QRadioTuner::Band band) const

    Identifies if a frequency \a band is supported.

    Returns true if the band is supported, and false if it is not.
*/

/*!
    \fn int QRadioTunerControl::frequency() const

    Returns the frequency a radio tuner is tuned to.
*/

/*!
    \fn int QRadioTunerControl::frequencyStep(QRadioTuner::Band band) const

    Returns the number of Hertz to increment the frequency by when stepping through frequencies
    within a given \a band.
*/

/*!
    \fn QPair<int,int> QRadioTunerControl::frequencyRange(QRadioTuner::Band band) const

    Returns a frequency \a band's minimum and maximum frequency.
*/

/*!
    \fn void QRadioTunerControl::setFrequency(int frequency)

    Sets the \a frequency a radio tuner is tuned to.
*/

/*!
    \fn bool QRadioTunerControl::isStereo() const

    Identifies if a radio tuner is receiving a stereo signal.

    Returns true if the tuner is receiving a stereo signal, and false if it is not.
*/

/*!
    \fn QRadioTuner::StereoMode QRadioTunerControl::stereoMode() const

    Returns a radio tuner's stereo mode.

    \sa QRadioTuner::StereoMode
*/

/*!
    \fn void QRadioTunerControl::setStereoMode(QRadioTuner::StereoMode mode)

    Sets a radio tuner's stereo \a mode.

    \sa QRadioTuner::StereoMode
*/

/*!
    \fn int QRadioTunerControl::signalStrength() const

    Return a radio tuner's current signal strength as a percentage.
*/

/*!
    \fn int QRadioTunerControl::volume() const

    Returns the volume of a radio tuner's audio output as a percentage.
*/

/*!
    \fn void QRadioTunerControl::setVolume(int volume)

    Sets the percentage \a volume of a radio tuner's audio output.
*/

/*!
    \fn bool QRadioTunerControl::isMuted() const

    Identifies if a radio tuner's audio output is muted.

    Returns true if the audio is muted, and false if it is not.
*/

/*!
    \fn void QRadioTunerControl::setMuted(bool muted)

    Sets the \a muted state of a radio tuner's audio output.
*/

/*!
    \fn bool QRadioTunerControl::isSearching() const

    Identifies if a radio tuner is currently scanning for signal.

    Returns true if the tuner is scanning, and false if it is not.
*/

/*!
    \fn bool QRadioTunerControl::isAntennaConnected() const

    Identifies if there is an antenna connected to the device.

    Returns true if there is a connected antenna, and false otherwise.
*/

/*!
    \fn  void QRadioTunerControl::searchForward()

    Starts a forward scan for a signal, starting from the current \l frequency().
*/

/*!
    \fn void QRadioTunerControl::searchBackward()

    Starts a backwards scan for a signal, starting from the current \l frequency().
*/

/*!
    \fn  void QRadioTunerControl::searchAllStations(QRadioTuner::SearchMode searchMode)

    Starts a scan through the whole frequency band searching all stations with a
    specific \a searchMode.
*/

/*!
    \fn void QRadioTunerControl::cancelSearch()

    Stops scanning for a signal.
*/

/*!
    \fn void QRadioTunerControl::start()

    Activate the radio device.
*/

/*!
    \fn QRadioTunerControl::stop()

    Deactivate the radio device.
*/

/*!
    \fn QRadioTuner::Error QRadioTunerControl::error() const

    Returns the error state of a radio tuner.
*/

/*!
    \fn QString QRadioTunerControl::errorString() const

    Returns a string describing a radio tuner's error state.
*/

/*!
    \fn void QRadioTunerControl::stateChanged(QRadioTuner::State state)

    Signals that the \a state of a radio tuner has changed.
*/


/*!
    \fn void QRadioTunerControl::frequencyChanged(int frequency)

    Signals that the \a frequency a radio tuner is tuned to has changed.
*/

/*!
    \fn void QRadioTunerControl::stereoStatusChanged(bool stereo)

    Signals that the \a stereo state of a radio tuner has changed.
*/

/*!
    \fn void QRadioTunerControl::searchingChanged(bool searching)

    Signals that the \a searching state of a radio tuner has changed.
*/

/*!
    \fn void QRadioTunerControl::signalStrengthChanged(int strength)

    Signals that the percentage \a strength of the signal received by a radio tuner has changed.
*/

/*!
    \fn void QRadioTunerControl::volumeChanged(int volume)

    Signals that the percentage \a volume of radio tuner's audio output has changed.
*/

/*!
    \fn void QRadioTunerControl::mutedChanged(bool muted)

    Signals that the \a muted state of a radio tuner's audio output has changed.
*/

/*!
    \fn void QRadioTunerControl::error(QRadioTuner::Error error)

    Signals that an \a error has occurred.
*/

/*!
    \fn void QRadioTunerControl::stationFound(int frequency, QString stationId)

    Signals that new station with \a frequency and \a stationId was found when scanning
*/

/*!
    \fn void QRadioTunerControl::antennaConnectedChanged(bool connectionStatus)

    Signals that the antenna has either been connected or disconnected as
    reflected with the \a connectionStatus.
*/

#include "moc_qradiotunercontrol.cpp"
QT_END_NAMESPACE

