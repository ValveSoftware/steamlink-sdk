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

#include "qradiodata.h"
#include "qmediaservice.h"
#include "qmediaobject_p.h"
#include "qradiodatacontrol.h"
#include "qmediaserviceprovider_p.h"

#include <QPair>


QT_BEGIN_NAMESPACE

static void qRegisterRadioDataMetaTypes()
{
    qRegisterMetaType<QRadioData::Error>();
    qRegisterMetaType<QRadioData::ProgramType>();
}

Q_CONSTRUCTOR_FUNCTION(qRegisterRadioDataMetaTypes)


/*!
    \class QRadioData
    \brief The QRadioData class provides interfaces to the RDS functionality of the system radio.

    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_radio

    The radio data object will emit signals for any changes in radio data. You can enable or disable
    alternative frequency with setAlternativeFrequenciesEnabled().

    You can get a QRadioData instance fromt the \l{QRadioTuner::radioData()}{radioData}
    property from a QRadioTuner instance.

    \snippet multimedia-snippets/media.cpp Radio data setup

    Alternatively, you can pass an instance of QRadioTuner to the constructor to QRadioData.

    \sa {Radio Overview}

*/


class QRadioDataPrivate
{
    Q_DECLARE_NON_CONST_PUBLIC(QRadioData)
public:
    QRadioDataPrivate();

    QMediaObject *mediaObject;
    QRadioDataControl* control;

    void _q_serviceDestroyed();

    QRadioData *q_ptr;
};

QRadioDataPrivate::QRadioDataPrivate()
    : mediaObject(0)
    , control(0)
{}

void QRadioDataPrivate::_q_serviceDestroyed()
{
    mediaObject = 0;
    control = 0;
}

/*!
    Constructs a radio data based on a \a mediaObject and \a parent.

    The \a mediaObject should be an instance of \l QRadioTuner. It is preferable to use the
    \l{QRadioTuner::radioData()}{radioData} property on a QRadioTuner instance to get an instance
    of QRadioData.

    During construction, this class is bound to the \a mediaObject using the
    \l{QMediaObject::bind()}{bind()} method.
*/

QRadioData::QRadioData(QMediaObject *mediaObject, QObject *parent)
    : QObject(parent)
    , d_ptr(new QRadioDataPrivate)
{
    Q_D(QRadioData);

    d->q_ptr = this;

    if (mediaObject)
        mediaObject->bind(this);
}

/*!
    Destroys a radio data.
*/

QRadioData::~QRadioData()
{
    Q_D(QRadioData);

    if (d->mediaObject)
        d->mediaObject->unbind(this);

    delete d_ptr;
}

/*!
  \reimp
*/
QMediaObject *QRadioData::mediaObject() const
{
    return d_func()->mediaObject;
}

/*!
  \reimp
*/
bool QRadioData::setMediaObject(QMediaObject *mediaObject)
{
    Q_D(QRadioData);

    if (d->mediaObject) {
        if (d->control) {
            disconnect(d->control, SIGNAL(stationIdChanged(QString)),
                       this, SIGNAL(stationIdChanged(QString)));
            disconnect(d->control, SIGNAL(programTypeChanged(QRadioData::ProgramType)),
                       this, SIGNAL(programTypeChanged(QRadioData::ProgramType)));
            disconnect(d->control, SIGNAL(programTypeNameChanged(QString)),
                       this, SIGNAL(programTypeNameChanged(QString)));
            disconnect(d->control, SIGNAL(stationNameChanged(QString)),
                       this, SIGNAL(stationNameChanged(QString)));
            disconnect(d->control, SIGNAL(radioTextChanged(QString)),
                       this, SIGNAL(radioTextChanged(QString)));
            disconnect(d->control, SIGNAL(alternativeFrequenciesEnabledChanged(bool)),
                       this, SIGNAL(alternativeFrequenciesEnabledChanged(bool)));
            disconnect(d->control, SIGNAL(error(QRadioData::Error)),
                       this, SIGNAL(error(QRadioData::Error)));

            QMediaService *service = d->mediaObject->service();
            service->releaseControl(d->control);
            disconnect(service, SIGNAL(destroyed()), this, SLOT(_q_serviceDestroyed()));
        }
    }

    d->mediaObject = mediaObject;

    if (d->mediaObject) {
        QMediaService *service = mediaObject->service();
        if (service) {
            d->control = qobject_cast<QRadioDataControl*>(service->requestControl(QRadioDataControl_iid));

            if (d->control) {
                connect(d->control, SIGNAL(stationIdChanged(QString)),
                        this, SIGNAL(stationIdChanged(QString)));
                connect(d->control, SIGNAL(programTypeChanged(QRadioData::ProgramType)),
                        this, SIGNAL(programTypeChanged(QRadioData::ProgramType)));
                connect(d->control, SIGNAL(programTypeNameChanged(QString)),
                        this, SIGNAL(programTypeNameChanged(QString)));
                connect(d->control, SIGNAL(stationNameChanged(QString)),
                        this, SIGNAL(stationNameChanged(QString)));
                connect(d->control, SIGNAL(radioTextChanged(QString)),
                        this, SIGNAL(radioTextChanged(QString)));
                connect(d->control, SIGNAL(alternativeFrequenciesEnabledChanged(bool)),
                        this, SIGNAL(alternativeFrequenciesEnabledChanged(bool)));
                connect(d->control, SIGNAL(error(QRadioData::Error)),
                        this, SIGNAL(error(QRadioData::Error)));

                connect(service, SIGNAL(destroyed()), this, SLOT(_q_serviceDestroyed()));

                return true;
            }
        }
    }

    // without QRadioDataControl discard the media object
    d->mediaObject = 0;
    d->control = 0;

    return false;
}

/*!
    Returns the availability of the radio data service.

    A long as there is a media service which provides radio functionality, then the
    \l{QMultimedia::AvailabilityStatus}{availability} will be that
    of the \l{QRadioTuner::availability()}{radio tuner}.
*/
QMultimedia::AvailabilityStatus QRadioData::availability() const
{
    Q_D(const QRadioData);

    if (d->control == 0)
        return QMultimedia::ServiceMissing;

    return d->mediaObject->availability();
}

/*!
    \property QRadioData::stationId
    \brief Current Program Identification

*/

QString QRadioData::stationId() const
{
    Q_D(const QRadioData);

    if (d->control != 0)
        return d->control->stationId();
    return QString();
}

/*!
    \property QRadioData::programType
    \brief Current Program Type

*/

QRadioData::ProgramType QRadioData::programType() const
{
    Q_D(const QRadioData);

    if (d->control != 0)
        return d->control->programType();

    return QRadioData::Undefined;
}

/*!
    \property QRadioData::programTypeName
    \brief Current Program Type Name

*/

QString QRadioData::programTypeName() const
{
    Q_D(const QRadioData);

    if (d->control != 0)
        return d->control->programTypeName();
    return QString();
}

/*!
    \property QRadioData::stationName
    \brief Current Program Service

*/

QString QRadioData::stationName() const
{
    Q_D(const QRadioData);

    if (d->control != 0)
        return d->control->stationName();
    return QString();
}

/*!
    \property QRadioData::radioText
    \brief Current Radio Text

*/

QString QRadioData::radioText() const
{
    Q_D(const QRadioData);

    if (d->control != 0)
        return d->control->radioText();
    return QString();
}

/*!
    \property QRadioData::alternativeFrequenciesEnabled
    \brief Is Alternative Frequency currently enabled

*/

bool QRadioData::isAlternativeFrequenciesEnabled() const
{
    Q_D(const QRadioData);

    if (d->control != 0)
        return d->control->isAlternativeFrequenciesEnabled();
    return false;
}

void QRadioData::setAlternativeFrequenciesEnabled( bool enabled )
{
    Q_D(const QRadioData);

    if (d->control != 0)
        return d->control->setAlternativeFrequenciesEnabled(enabled);
}

/*!
    Returns the error state of a radio data.

    \sa errorString()
*/

QRadioData::Error QRadioData::error() const
{
    Q_D(const QRadioData);

    if (d->control != 0)
        return d->control->error();
    return QRadioData::ResourceError;
}

/*!
    Returns a description of a radio data's error state.

    \sa error()
*/
QString QRadioData::errorString() const
{
    Q_D(const QRadioData);

    if (d->control != 0)
        return d->control->errorString();
    return QString();
}

/*!
    \fn void QRadioData::stationIdChanged(QString stationId)

    Signals that the Program Identification code has changed to \a stationId
*/

/*!
    \fn void QRadioData::programTypeChanged(QRadioData::ProgramType programType)

    Signals that the Program Type code has changed to \a programType
*/

/*!
    \fn void QRadioData::programTypeNameChanged(QString programTypeName)

    Signals that the Program Type Name has changed to \a programTypeName
*/

/*!
    \fn void QRadioData::stationNameChanged(QString stationName)

    Signals that the Program Service has changed to \a stationName
*/

/*!
    \fn void QRadioData::alternativeFrequenciesEnabledChanged(bool enabled)

    Signals that automatically tuning to alternative frequencies has been
    enabled or disabled according to \a enabled.
*/

/*!
    \fn void QRadioData::radioTextChanged(QString radioText)

    Signals that the Radio Text property has changed to \a radioText
*/

/*!
    \fn void QRadioData::error(QRadioData::Error error)

    Signals that an \a error occurred.
*/

/*!
    \enum QRadioData::Error

    Enumerates radio data error conditions.

    \value NoError         No errors have occurred.
    \value ResourceError   There is no radio service available.
    \value OpenError       Unable to open radio device.
    \value OutOfRangeError An attempt to set a frequency or band that is not supported by radio device.
*/

/*!
    \enum QRadioData::ProgramType

    This property holds the type of the currently playing program as transmitted
    by the radio station. The value can be any one of the values defined in the
    table below.

    \value Undefined
    \value News
    \value CurrentAffairs
    \value Information
    \value Sport
    \value Education
    \value Drama
    \value Culture
    \value Science
    \value Varied
    \value PopMusic
    \value RockMusic
    \value EasyListening
    \value LightClassical
    \value SeriousClassical
    \value OtherMusic
    \value Weather
    \value Finance
    \value ChildrensProgrammes
    \value SocialAffairs
    \value Religion
    \value PhoneIn
    \value Travel
    \value Leisure
    \value JazzMusic
    \value CountryMusic
    \value NationalMusic
    \value OldiesMusic
    \value FolkMusic
    \value Documentary
    \value AlarmTest
    \value Alarm
    \value Talk
    \value ClassicRock
    \value AdultHits
    \value SoftRock
    \value Top40
    \value Soft
    \value Nostalgia
    \value Classical
    \value RhythmAndBlues
    \value SoftRhythmAndBlues
    \value Language
    \value ReligiousMusic
    \value ReligiousTalk
    \value Personality
    \value Public
    \value College
*/

#include "moc_qradiodata.cpp"
QT_END_NAMESPACE

