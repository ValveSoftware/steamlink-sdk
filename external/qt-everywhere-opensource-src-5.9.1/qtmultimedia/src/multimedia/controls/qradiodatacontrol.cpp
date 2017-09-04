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
#include "qradiodatacontrol.h"
#include "qmediacontrol_p.h"

QT_BEGIN_NAMESPACE


/*!
    \class QRadioDataControl
    \inmodule QtMultimedia


    \ingroup multimedia_control


    \brief The QRadioDataControl class provides access to the RDS functionality of the
    radio in the QMediaService.

    The functionality provided by this control is exposed to application code
    through the QRadioData class.

    The interface name of QRadioDataControl is \c org.qt-project.qt.radiodatacontrol/5.0 as
    defined in QRadioDataControl_iid.

    \sa QMediaService::requestControl(), QRadioData
*/

/*!
    \macro QRadioDataControl_iid

    \c org.qt-project.qt.radiodatacontrol/5.0

    Defines the interface name of the QRadioDataControl class.

    \relates QRadioDataControl
*/

/*!
    Constructs a radio data control with the given \a parent.
*/

QRadioDataControl::QRadioDataControl(QObject *parent):
    QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
    Destroys a radio data control.
*/

QRadioDataControl::~QRadioDataControl()
{
}

/*!
    \fn QRadioData::Error QRadioDataControl::error() const

    Returns the error state of a radio data.
*/

/*!
    \fn QString QRadioDataControl::errorString() const

    Returns a string describing a radio data's error state.
*/

/*!
    \fn void QRadioDataControl::error(QRadioData::Error error)

    Signals that an \a error has occurred.
*/

/*!
    \fn QString QRadioDataControl::stationId() const

    Returns the current Program Identification
*/

/*!
    \fn QRadioData::ProgramType QRadioDataControl::programType() const

    Returns the current Program Type
*/

/*!
    \fn QString QRadioDataControl::programTypeName() const

    Returns the current Program Type Name
*/

/*!
    \fn QString QRadioDataControl::stationName() const

    Returns the current Program Service
*/

/*!
    \fn QString QRadioDataControl::radioText() const

    Returns the current Radio Text
*/

/*!
    \fn void QRadioDataControl::setAlternativeFrequenciesEnabled(bool enabled)

    Sets the Alternative Frequency to \a enabled
*/

/*!
    \fn bool QRadioDataControl::isAlternativeFrequenciesEnabled() const

    Returns true if Alternative Frequency is currently enabled
*/

/*!
    \fn QRadioDataControl::alternativeFrequenciesEnabledChanged(bool enabled)

    Signals that the alternative frequencies setting has changed to the value of \a enabled.
*/

/*!
    \fn void QRadioDataControl::stationIdChanged(QString stationId)

    Signals that the Program Identification \a stationId has changed
*/

/*!
    \fn void QRadioDataControl::programTypeChanged(QRadioData::ProgramType programType)

    Signals that the Program Type \a programType has changed
*/

/*!
    \fn void QRadioDataControl::programTypeNameChanged(QString programTypeName)

    Signals that the Program Type Name \a programTypeName has changed
*/

/*!
    \fn void QRadioDataControl::stationNameChanged(QString stationName)

    Signals that the Program Service \a stationName has changed
*/

/*!
    \fn void QRadioDataControl::radioTextChanged(QString radioText)

    Signals that the Radio Text \a radioText has changed
*/

#include "moc_qradiodatacontrol.cpp"
QT_END_NAMESPACE

