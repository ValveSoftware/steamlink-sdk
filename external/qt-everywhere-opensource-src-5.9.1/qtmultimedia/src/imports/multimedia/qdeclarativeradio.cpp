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

#include "qdeclarativeradio_p.h"
#include "qdeclarativeradiodata_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype Radio
    \instantiates QDeclarativeRadio
    \inqmlmodule QtMultimedia
    \brief Access radio functionality from a QML application.
    \ingroup multimedia_qml
    \ingroup multimedia_radio_qml
    \inherits Item

    \qml
    Rectangle {
        width: 320
        height: 480

        Radio {
            id: radio
            band: Radio.FM
        }

        MouseArea {
            x: 0; y: 0
            height: parent.height
            width: parent.width / 2

            onClicked: radio.scanDown()
        }

        MouseArea {
            x: parent.width / 2; y: 0
            height: parent.height
            width: parent.width / 2

            onClicked: radio.scanUp()
        }
    }
    \endqml

    You can use \c Radio to tune the radio and get information about the signal.
    You can also use the Radio to get information about tuning, for instance the
    frequency steps supported for tuning.

    The corresponding \l RadioData gives RDS information about the
    current radio station. The best way to access the RadioData for
    the current Radio is to use the \c radioData property.

    \sa {Radio Overview}

*/


QDeclarativeRadio::QDeclarativeRadio(QObject *parent) :
    QObject(parent)
{
    m_radioTuner = new QRadioTuner(this);
    m_radioData = new QDeclarativeRadioData(m_radioTuner, this);

    connect(m_radioTuner, SIGNAL(stateChanged(QRadioTuner::State)), this, SLOT(_q_stateChanged(QRadioTuner::State)));
    connect(m_radioTuner, SIGNAL(bandChanged(QRadioTuner::Band)), this, SLOT(_q_bandChanged(QRadioTuner::Band)));

    connect(m_radioTuner, SIGNAL(frequencyChanged(int)), this, SIGNAL(frequencyChanged(int)));
    connect(m_radioTuner, SIGNAL(stereoStatusChanged(bool)), this, SIGNAL(stereoStatusChanged(bool)));
    connect(m_radioTuner, SIGNAL(searchingChanged(bool)), this, SIGNAL(searchingChanged(bool)));
    connect(m_radioTuner, SIGNAL(signalStrengthChanged(int)), this, SIGNAL(signalStrengthChanged(int)));
    connect(m_radioTuner, SIGNAL(volumeChanged(int)), this, SIGNAL(volumeChanged(int)));
    connect(m_radioTuner, SIGNAL(mutedChanged(bool)), this, SIGNAL(mutedChanged(bool)));
    connect(m_radioTuner, SIGNAL(stationFound(int,QString)), this, SIGNAL(stationFound(int,QString)));
    connect(m_radioTuner, SIGNAL(antennaConnectedChanged(bool)), this, SIGNAL(antennaConnectedChanged(bool)));
    connect(m_radioTuner, SIGNAL(availabilityChanged(QMultimedia::AvailabilityStatus)), this, SLOT(_q_availabilityChanged(QMultimedia::AvailabilityStatus)));

    connect(m_radioTuner, SIGNAL(error(QRadioTuner::Error)), this, SLOT(_q_error(QRadioTuner::Error)));
}

QDeclarativeRadio::~QDeclarativeRadio()
{
}

/*!
    \qmlproperty enumeration QtMultimedia::Radio::state

    This property holds the current state of the Radio.

    \table
    \header \li Value \li Description
    \row \li ActiveState
        \li The radio is started and active

    \row \li StoppedState
        \li The radio is stopped

    \endtable

    \sa start, stop
*/
QDeclarativeRadio::State QDeclarativeRadio::state() const
{
    return static_cast<QDeclarativeRadio::State>(m_radioTuner->state());
}

/*!
    \qmlproperty enumeration QtMultimedia::Radio::band

    This property holds the frequency band used for the radio, which can be specified as
    any one of the values in the table below.

    \table
    \header \li Value \li Description
    \row \li AM
        \li 520 to 1610 kHz, 9 or 10kHz channel spacing, extended 1610 to 1710 kHz

    \row \li FM
        \li 87.5 to 108.0 MHz, except Japan 76-90 MHz

    \row \li SW
        \li 1.711 to 30.0 MHz, divided into 15 bands. 5kHz channel spacing

    \row \li LW
        \li 148.5 to 283.5 kHz, 9kHz channel spacing (Europe, Africa, Asia)

    \row \li FM2
        \li range not defined, used when area supports more than one FM range

    \endtable
*/
QDeclarativeRadio::Band QDeclarativeRadio::band() const
{
    return static_cast<QDeclarativeRadio::Band>(m_radioTuner->band());
}

/*!
    \qmlproperty int QtMultimedia::Radio::frequency

    Sets the frequency in Hertz that the radio is tuned to. The frequency must be within the frequency
    range for the current band, otherwise it will be changed to be within the frequency range.

    \sa maximumFrequency, minimumFrequency
*/
int QDeclarativeRadio::frequency() const
{
    return m_radioTuner->frequency();
}

/*!
    \qmlproperty enumeration QtMultimedia::Radio::stereoMode

    This property holds the stereo mode of the radio, which can be set to any one of the
    values in the table below.

    \table
    \header \li Value \li Description
    \row \li Auto
        \li Uses stereo mode matching the station

    \row \li ForceStereo
        \li Forces the radio to play the station in stereo, converting the sound signal if necessary

    \row \li ForceMono
        \li Forces the radio to play the station in mono, converting the sound signal if necessary

    \endtable
*/
QDeclarativeRadio::StereoMode QDeclarativeRadio::stereoMode() const
{
    return static_cast<QDeclarativeRadio::StereoMode>(m_radioTuner->stereoMode());
}

/*!
    \qmlproperty int QtMultimedia::Radio::volume

    Set this property to control the volume of the radio. The valid range of the volume is from 0 to 100.
*/
int QDeclarativeRadio::volume() const
{
    return m_radioTuner->volume();
}

/*!
    \qmlproperty bool QtMultimedia::Radio::muted

    This property reflects whether the radio is muted or not.
*/
bool QDeclarativeRadio::muted() const
{
    return m_radioTuner->isMuted();
}

/*!
    \qmlproperty bool QtMultimedia::Radio::stereo

    This property holds whether the radio is receiving a stereo signal or not. If \l stereoMode is
    set to ForceMono the value will always be false. Likewise, it will always be true if stereoMode
    is set to ForceStereo.

    \sa stereoMode
*/
bool QDeclarativeRadio::stereo() const
{
    return m_radioTuner->isStereo();
}

/*!
    \qmlproperty int QtMultimedia::Radio::signalStrength

    The strength of the current radio signal as a percentage where 0% equals no signal, and 100% is a
    very good signal.
*/
int QDeclarativeRadio::signalStrength() const
{
    return m_radioTuner->signalStrength();
}

/*!
    \qmlproperty bool QtMultimedia::Radio::searching

    This property is true if the radio is currently searching for radio stations, for instance using the \l scanUp,
    \l scanDown, and \l searchAllStations methods. Once the search completes, or if it is cancelled using
    \l cancelScan, this property will be false.
*/
bool QDeclarativeRadio::searching() const
{
    return m_radioTuner->isSearching();
}

/*!
    \qmlproperty int QtMultimedia::Radio::frequencyStep

    The number of Hertz for each step when tuning the radio manually. The value is for the current \l band.
 */
int QDeclarativeRadio::frequencyStep() const
{
    return m_radioTuner->frequencyStep(m_radioTuner->band());
}

/*!
    \qmlproperty int QtMultimedia::Radio::minimumFrequency

    The minimum frequency for the current \l band.
 */
int QDeclarativeRadio::minimumFrequency() const
{
    return m_radioTuner->frequencyRange(m_radioTuner->band()).first;
}

/*!
    \qmlproperty int QtMultimedia::Radio::maximumFrequency

    The maximum frequency for the current \l band.
 */
int QDeclarativeRadio::maximumFrequency() const
{
    return m_radioTuner->frequencyRange(m_radioTuner->band()).second;
}

/*!
    \qmlproperty int QtMultimedia::Radio::antennaConnected

    This property is true if there is an antenna connected. Otherwise it will be false.
 */
bool QDeclarativeRadio::isAntennaConnected() const
{
    return m_radioTuner->isAntennaConnected();
}

/*!
    \qmlproperty enumeration QtMultimedia::Radio::availability

    Returns the availability state of the radio.

    This is one of:

    \table
    \header \li Value \li Description
    \row \li Available
        \li The radio is available to use
    \row \li Busy
        \li The radio is usually available to use, but is currently busy.
           This can happen when some other process needs to use the audio
           hardware.
    \row \li Unavailable
        \li The radio is not available to use (there may be no radio
           hardware)
    \row \li ResourceMissing
        \li There is one or more resources missing, so the radio cannot
           be used.  It may be possible to try again at a later time.  This
           can occur if there is no antenna connected - see the \l antennaConnected
           property as well.
    \endtable
 */
QDeclarativeRadio::Availability QDeclarativeRadio::availability() const
{
    return Availability(m_radioTuner->availability());
}

void QDeclarativeRadio::setBand(QDeclarativeRadio::Band band)
{
    m_radioTuner->setBand(static_cast<QRadioTuner::Band>(band));
}

void QDeclarativeRadio::setFrequency(int frequency)
{
    m_radioTuner->setFrequency(frequency);
}

void QDeclarativeRadio::setStereoMode(QDeclarativeRadio::StereoMode stereoMode)
{
    m_radioTuner->setStereoMode(static_cast<QRadioTuner::StereoMode>(stereoMode));
}

void QDeclarativeRadio::setVolume(int volume)
{
    m_radioTuner->setVolume(volume);
}

void QDeclarativeRadio::setMuted(bool muted)
{
    m_radioTuner->setMuted(muted);
}

/*!
    \qmlmethod QtMultimedia::Radio::cancelScan()

    Cancel the current scan. Will also cancel a search started with \l searchAllStations.
 */
void QDeclarativeRadio::cancelScan()
{
    m_radioTuner->cancelSearch();
}

/*!
    \qmlmethod QtMultimedia::Radio::scanDown()

    Searches backward in the frequency range for the current band.
 */
void QDeclarativeRadio::scanDown()
{
    m_radioTuner->searchBackward();
}

/*!
    \qmlmethod QtMultimedia::Radio::scanUp()

    Searches forward in the frequency range for the current band.
 */
void QDeclarativeRadio::scanUp()
{
    m_radioTuner->searchForward();
}

/*!
    \qmlmethod QtMultimedia::Radio::searchAllStations(enumeration searchMode)

    Start searching the complete frequency range for the current band, and save all the
    radio stations found. The search mode can be either of the values described in the
    table below.

    \table
    \header \li Value \li Description
    \row \li SearchFast
        \li Stores each radio station for later retrival and tuning

    \row \li SearchGetStationId
        \li Does the same as SearchFast, but also emits the station Id with the \l stationFound signal.

    \endtable

    The snippet below uses \c searchAllStations with \c SearchGetStationId to receive all the radio
    stations in the current band, and store them in a ListView. The station Id is shown to the user
    and if the user presses a station, the radio is tuned to this station.

    \qml
    Item {
        width: 640
        height: 360

        Radio {
            id: radio
            onStationFound: radioStations.append({"frequency": frequency, "stationId": stationId})
        }

        ListModel {
            id: radioStations
        }

        ListView {
            model: radioStations
            delegate: Rectangle {
                    MouseArea {
                        anchors.fill: parent
                        onClicked: radio.frequency = frequency
                    }

                    Text {
                        anchors.fill: parent
                        text: stationId
                    }
                }
        }

        Rectangle {
            MouseArea {
                anchors.fill: parent
                onClicked: radio.searchAllStations(Radio.SearchGetStationId)
            }
        }
    }
    \endqml
 */
void QDeclarativeRadio::searchAllStations(QDeclarativeRadio::SearchMode searchMode)
{
    m_radioTuner->searchAllStations(static_cast<QRadioTuner::SearchMode>(searchMode));
}

/*!
    \qmlmethod QtMultimedia::Radio::tuneDown()

    Decrements the frequency by the frequency step for the current band. If the frequency is already set
    to the minimum frequency, calling this function has no effect.

    \sa band, frequencyStep, minimumFrequency
 */
void QDeclarativeRadio::tuneDown()
{
    int f = frequency();
    f = f - frequencyStep();
    setFrequency(f);
}

/*!
    \qmlmethod QtMultimedia::Radio::tuneUp()

    Increments the frequency by the frequency step for the current band. If the frequency is already set
    to the maximum frequency, calling this function has no effect.

    \sa band, frequencyStep, maximumFrequency
 */
void QDeclarativeRadio::tuneUp()
{
    int f = frequency();
    f = f + frequencyStep();
    setFrequency(f);
}

/*!
    \qmlmethod QtMultimedia::Radio::start()

    Starts the radio. If the radio is available, as determined by the \l availability property,
    this will result in the \l state becoming \c ActiveState.
 */
void QDeclarativeRadio::start()
{
    m_radioTuner->start();
}

/*!
    \qmlmethod QtMultimedia::Radio::stop()

    Stops the radio. After calling this method the \l state will be \c StoppedState.
 */
void QDeclarativeRadio::stop()
{
    m_radioTuner->stop();
}

void QDeclarativeRadio::_q_stateChanged(QRadioTuner::State state)
{
    emit stateChanged(static_cast<QDeclarativeRadio::State>(state));
}

void QDeclarativeRadio::_q_bandChanged(QRadioTuner::Band band)
{
    emit bandChanged(static_cast<QDeclarativeRadio::Band>(band));
}

void QDeclarativeRadio::_q_error(QRadioTuner::Error errorCode)
{
    emit error(static_cast<QDeclarativeRadio::Error>(errorCode));
    emit errorChanged();
}

void QDeclarativeRadio::_q_availabilityChanged(QMultimedia::AvailabilityStatus availability)
{
    emit availabilityChanged(Availability(availability));
}

/*!
    \qmlsignal QtMultimedia::Radio::stationFound(int frequency, string stationId)

    This signal is emitted when a new radio station is found. This signal is only emitted
    if \l searchAllStations is called with \c SearchGetStationId.

    The \a frequency is returned in Hertz, and the \a stationId corresponds to the station Id
    in the \l RadioData for this radio station.

    The corresponding handler is \c onStationFound.
  */

QT_END_NAMESPACE
