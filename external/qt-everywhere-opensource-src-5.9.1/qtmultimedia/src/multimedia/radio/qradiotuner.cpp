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

#include "qradiotuner.h"
#include "qmediaservice.h"
#include "qmediaobject_p.h"
#include "qradiotunercontrol.h"
#include "qradiodata.h"
#include "qmediaserviceprovider_p.h"

#include <QPair>


QT_BEGIN_NAMESPACE


static void qRegisterRadioTunerMetaTypes()
{
    qRegisterMetaType<QRadioTuner::Band>();
    qRegisterMetaType<QRadioTuner::Error>();
    qRegisterMetaType<QRadioTuner::SearchMode>();
    qRegisterMetaType<QRadioTuner::State>();
    qRegisterMetaType<QRadioTuner::StereoMode>();
}

Q_CONSTRUCTOR_FUNCTION(qRegisterRadioTunerMetaTypes)


/*!
    \class QRadioTuner
    \brief The QRadioTuner class provides an interface to the systems analog radio device.

    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_radio

    You can control the systems analog radio device using this interface, for example:

    \snippet multimedia-snippets/media.cpp Radio tuner

    The radio object will emit signals for any changes in state such as:
    bandChanged(), frequencyChanged(), stereoStatusChanged(), searchingChanged(),
    signalStrengthChanged(), volumeChanged(), mutedChanged().

    You can change between the frequency bands using setBand() however it is recommended
    that you check to make sure the band is available first using isBandSupported().

    \sa QRadioData, {Radio Overview}
*/


class QRadioTunerPrivate : public QMediaObjectPrivate
{
public:
    QRadioTunerPrivate():provider(0), control(0), radioData(0) {}
    QMediaServiceProvider *provider;
    QRadioTunerControl* control;
    QRadioData *radioData;
};



/*!
    Constructs a radio tuner based on a media service allocated by the default
    media service provider.

    The \a parent is passed to QMediaObject.
*/

QRadioTuner::QRadioTuner(QObject *parent):
    QMediaObject(*new QRadioTunerPrivate,
                 parent,
                 QMediaServiceProvider::defaultServiceProvider()->requestService(Q_MEDIASERVICE_RADIO))
{
    Q_D(QRadioTuner);

    d->provider = QMediaServiceProvider::defaultServiceProvider();

    if (d->service != 0) {
        d->control = qobject_cast<QRadioTunerControl*>(d->service->requestControl(QRadioTunerControl_iid));
        if (d->control != 0) {
            connect(d->control, SIGNAL(stateChanged(QRadioTuner::State)), SIGNAL(stateChanged(QRadioTuner::State)));
            connect(d->control, SIGNAL(bandChanged(QRadioTuner::Band)), SIGNAL(bandChanged(QRadioTuner::Band)));
            connect(d->control, SIGNAL(frequencyChanged(int)), SIGNAL(frequencyChanged(int)));
            connect(d->control, SIGNAL(stereoStatusChanged(bool)), SIGNAL(stereoStatusChanged(bool)));
            connect(d->control, SIGNAL(searchingChanged(bool)), SIGNAL(searchingChanged(bool)));
            connect(d->control, SIGNAL(signalStrengthChanged(int)), SIGNAL(signalStrengthChanged(int)));
            connect(d->control, SIGNAL(volumeChanged(int)), SIGNAL(volumeChanged(int)));
            connect(d->control, SIGNAL(mutedChanged(bool)), SIGNAL(mutedChanged(bool)));
            connect(d->control, SIGNAL(stationFound(int,QString)), SIGNAL(stationFound(int,QString)));
            connect(d->control, SIGNAL(antennaConnectedChanged(bool)), SIGNAL(antennaConnectedChanged(bool)));
            connect(d->control, SIGNAL(error(QRadioTuner::Error)), SIGNAL(error(QRadioTuner::Error)));
        }

        d->radioData = new QRadioData(this, this);
    }
}

/*!
    Destroys a radio tuner.
*/

QRadioTuner::~QRadioTuner()
{
    Q_D(QRadioTuner);

    if (d->radioData)
        delete d->radioData;

    if (d->service && d->control)
        d->service->releaseControl(d->control);

    d->provider->releaseService(d->service);
}

/*!
    Returns the availability of the radio tuner.
*/
QMultimedia::AvailabilityStatus QRadioTuner::availability() const
{
    if (d_func()->control == 0)
        return QMultimedia::ServiceMissing;

    if (!d_func()->control->isAntennaConnected())
        return QMultimedia::ResourceError;

    return QMediaObject::availability();
}

/*!
    \property QRadioTuner::state
    Return the current radio tuner state.

    \sa QRadioTuner::State
*/

QRadioTuner::State QRadioTuner::state() const
{
    return d_func()->control ?
            d_func()->control->state() : QRadioTuner::StoppedState;
}

/*!
    \property QRadioTuner::band
    \brief the frequency band a radio tuner is tuned to.

    \sa QRadioTuner::Band
*/

QRadioTuner::Band QRadioTuner::band() const
{
    Q_D(const QRadioTuner);

    if (d->control != 0)
        return d->control->band();

    return QRadioTuner::FM;
}

/*!
    \property QRadioTuner::frequency
    \brief the frequency in Hertz a radio tuner is tuned to.
*/

int QRadioTuner::frequency() const
{
    Q_D(const QRadioTuner);

    if (d->control != 0)
        return d->control->frequency();

    return 0;
}

/*!
    Returns the number of Hertz to increment the frequency by when stepping through frequencies
    within a given \a band.
*/

int QRadioTuner::frequencyStep(QRadioTuner::Band band) const
{
    Q_D(const QRadioTuner);

    if(d->control != 0)
        return d->control->frequencyStep(band);

    return 0;
}

/*!
    Returns a frequency \a band's minimum and maximum frequency.
*/

QPair<int,int> QRadioTuner::frequencyRange(QRadioTuner::Band band) const
{
    Q_D(const QRadioTuner);

    if(d->control != 0)
        return d->control->frequencyRange(band);

    return qMakePair<int,int>(0,0);
}

/*!
    \property QRadioTuner::stereo
    \brief whether a radio tuner is receiving a stereo signal.
*/

bool QRadioTuner::isStereo() const
{
    Q_D(const QRadioTuner);

    if (d->control != 0)
        return d->control->isStereo();

    return false;
}


/*!
    \property QRadioTuner::stereoMode
    \brief the stereo mode of a radio tuner.
*/

QRadioTuner::StereoMode QRadioTuner::stereoMode() const
{
    Q_D(const QRadioTuner);

    if (d->control != 0)
        return d->control->stereoMode();

    return QRadioTuner::Auto;
}

void QRadioTuner::setStereoMode(QRadioTuner::StereoMode mode)
{
    Q_D(QRadioTuner);

    if (d->control != 0)
        return d->control->setStereoMode(mode);
}

/*!
    Identifies if a frequency \a band is supported by a radio tuner.

    Returns true if the band is supported, and false if it is not.
*/

bool QRadioTuner::isBandSupported(QRadioTuner::Band band) const
{
    Q_D(const QRadioTuner);

    if (d->control != 0)
        return d->control->isBandSupported(band);

    return false;
}

/*!
    Activate the radio device.
*/

void QRadioTuner::start()
{
    Q_D(const QRadioTuner);

    if (d->control != 0)
        d->control->start();
}

/*!
    Deactivate the radio device.
*/

void QRadioTuner::stop()
{
    Q_D(const QRadioTuner);

    if (d->control != 0)
        d->control->stop();
}

/*!
    \property QRadioTuner::signalStrength
    \brief the strength of the current radio signal as a percentage.
*/

int QRadioTuner::signalStrength() const
{
    Q_D(const QRadioTuner);

    if (d->control != 0)
        return d->control->signalStrength();

    return 0;
}

/*!
    \property QRadioTuner::volume
    \brief the volume of a radio tuner's audio output as a percentage.
*/


int QRadioTuner::volume() const
{
    Q_D(const QRadioTuner);

    if (d->control != 0)
        return d->control->volume();

    return 0;
}

/*!
    \property QRadioTuner::muted
    \brief whether a radio tuner's audio output is muted.
*/

bool QRadioTuner::isMuted() const
{
    Q_D(const QRadioTuner);

    if (d->control != 0)
        return d->control->isMuted();

    return false;
}

/*!
    Sets a radio tuner's frequency \a band.

    Changing the band will reset the \l frequency to the new band's minimum frequency.
*/

void QRadioTuner::setBand(QRadioTuner::Band band)
{
    Q_D(QRadioTuner);

    if (d->control != 0)
        d->control->setBand(band);
}

/*!
    Sets a radio tuner's \a frequency.

    If the tuner is set to a frequency outside the current \l band, the band will be changed to
    one occupied by the new frequency.
*/

void QRadioTuner::setFrequency(int frequency)
{
    Q_D(QRadioTuner);

    if (d->control != 0)
        d->control->setFrequency(frequency);
}

void QRadioTuner::setVolume(int volume)
{
    Q_D(QRadioTuner);

    if (d->control != 0)
        d->control->setVolume(volume);
}

void QRadioTuner::setMuted(bool muted)
{
    Q_D(QRadioTuner);

    if (d->control != 0)
        d->control->setMuted(muted);
}

/*!
    \property QRadioTuner::searching
    \brief whether a radio tuner is currently scanning for a signal.

    \sa searchForward(), searchBackward(), cancelSearch()
*/

bool QRadioTuner::isSearching() const
{
    Q_D(const QRadioTuner);

    if (d->control != 0)
        return d->control->isSearching();

    return false;
}

/*!
    \property QRadioTuner::antennaConnected
    \brief whether there is an antenna connected
*/
bool QRadioTuner::isAntennaConnected() const
{
    Q_D(const QRadioTuner);

    if (d->control != 0)
        return d->control->isAntennaConnected();

    return false;
}

/*!
    Starts a forward scan for a signal, starting from the current \l frequency.

    \sa searchBackward(), cancelSearch(), searching
*/

void QRadioTuner::searchForward()
{
    Q_D(QRadioTuner);

    if (d->control != 0)
        d->control->searchForward();
}

/*!
    Starts a backwards scan for a signal, starting from the current \l frequency.

    \sa searchForward(), cancelSearch(), searching
*/

void QRadioTuner::searchBackward()
{
    Q_D(QRadioTuner);

    if (d->control != 0)
        d->control->searchBackward();
}

/*!
    \enum QRadioTuner::SearchMode

    Enumerates how the radio tuner should search for stations.

    \value SearchFast           Use only signal strength when searching.
    \value SearchGetStationId   After finding a strong signal, wait for the RDS station id (PI) before continuing.
*/

/*!
    Search all stations in current band

    Emits QRadioTuner::stationFound(int, QString) for every found station.
    After searching is completed, QRadioTuner::searchingChanged(bool) is
    emitted (false). If \a searchMode is set to SearchGetStationId, searching
    waits for station id (PI) on each frequency.

    \sa searchForward(), searchBackward(), searching
*/

void QRadioTuner::searchAllStations(QRadioTuner::SearchMode searchMode)
{
    Q_D(const QRadioTuner);

    if (d->control != 0)
        d->control->searchAllStations(searchMode);
}

/*!
    Stops scanning for a signal.

    \sa searchForward(), searchBackward(), searching
*/

void QRadioTuner::cancelSearch()
{
    Q_D(QRadioTuner);

    if (d->control != 0)
        d->control->cancelSearch();
}

/*!
    Returns the error state of a radio tuner.

    \sa errorString()
*/

QRadioTuner::Error QRadioTuner::error() const
{
    Q_D(const QRadioTuner);

    if (d->control != 0)
        return d->control->error();

    return QRadioTuner::ResourceError;
}

/*!
    Returns a description of a radio tuner's error state.

    \sa error()
*/

QString QRadioTuner::errorString() const
{
    Q_D(const QRadioTuner);

    if (d->control != 0)
        return d->control->errorString();

    return QString();
}

/*!
    \property QRadioTuner::radioData
    \brief holds an instance of \l QRadioData

    The instance of QRadioData is already bound to this instance of QRadioTuner.
*/
QRadioData *QRadioTuner::radioData() const
{
    return d_func()->radioData;
}


/*!
    \fn void QRadioTuner::bandChanged(QRadioTuner::Band band)

    Signals a radio tuner's \a band has changed.
*/

/*!
    \fn void QRadioTuner::frequencyChanged(int frequency)

    Signals that the \a frequency a radio tuner is tuned to has changed.
*/

/*!
    \fn void QRadioTuner::mutedChanged(bool muted)

    Signals that the \a muted state of a radio tuner's audio output has changed.
*/

/*!
    \fn void QRadioTuner::volumeChanged(int volume)

    Signals that the \a volume of a radio tuner's audio output has changed.
*/

/*!
    \fn void QRadioTuner::searchingChanged(bool searching)

    Signals that the \a searching state of a radio tuner has changed.
*/

/*!
    \fn void QRadioTuner::stereoStatusChanged(bool stereo)

    Signals that the \a stereo state of a radio tuner has changed.
*/

/*!
    \fn void QRadioTuner::signalStrengthChanged(int strength)

    Signals that the \a strength of the signal received by a radio tuner has changed.
*/

/*!
    \fn void QRadioTuner::stationFound(int frequency, QString stationId)

    Signals that a station was found in \a frequency with \a stationId Program
    Identification code.
*/

/*!
    \fn void QRadioTuner::error(QRadioTuner::Error error)

    Signals that an \a error occurred.
*/

/*!
    \enum QRadioTuner::State

    Enumerates radio tuner states.

    \value ActiveState The tuner is started and active.
    \value StoppedState The tuner device is stopped.
*/


/*!
    \enum QRadioTuner::Band

    Enumerates radio frequency bands.

    \value AM 520 to 1610 kHz, 9 or 10kHz channel spacing, extended 1610 to 1710 kHz
    \value FM 87.5 to 108.0 MHz, except Japan 76-90 MHz
    \value SW 1.711 to 30.0 MHz, divided into 15 bands. 5kHz channel spacing
    \value LW 148.5 to 283.5 kHz, 9kHz channel spacing (Europe, Africa, Asia)
    \value FM2 range not defined, used when area supports more than one FM range.
*/

/*!
    \enum QRadioTuner::Error

    Enumerates radio tuner error conditions.

    \value NoError         No errors have occurred.
    \value ResourceError   There is no radio service available.
    \value OpenError       Unable to open radio device.
    \value OutOfRangeError An attempt to set a frequency or band that is not supported by radio device.
*/

/*!
    \enum QRadioTuner::StereoMode

    Enumerates radio tuner policy for receiving stereo signals.

    \value ForceStereo Provide stereo mode, converting if required.
    \value ForceMono   Provide mono mode, converting if required.
    \value Auto        Uses the stereo mode matching the station.
*/

/*! \fn void QRadioTuner::stateChanged(QRadioTuner::State state)
  This signal is emitted when the state changes to \a state.
 */

#include "moc_qradiotuner.cpp"
QT_END_NAMESPACE

