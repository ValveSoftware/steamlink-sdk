/***************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include "qlowenergyservicedata.h"

#include "qlowenergycharacteristicdata.h"

#include <QtCore/qloggingcategory.h>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT)

struct QLowEnergyServiceDataPrivate : public QSharedData
{
    QLowEnergyServiceDataPrivate() : type(QLowEnergyServiceData::ServiceTypePrimary) {}

    QLowEnergyServiceData::ServiceType type;
    QBluetoothUuid uuid;
    QList<QLowEnergyService *> includedServices;
    QList<QLowEnergyCharacteristicData> characteristics;
};


/*!
    \since 5.7
    \class QLowEnergyServiceData
    \brief The QLowEnergyServiceData class is used to set up GATT service data.
    \inmodule QtBluetooth
    \ingroup shared

    An Object of this class provides a service to be added to a GATT server via
    \l QLowEnergyController::addService().
*/

/*!
   \enum QLowEnergyServiceData::ServiceType
   The type of GATT service.

   \value ServiceTypePrimary
       The service is a primary service.
   \value ServiceTypeSecondary
       The service is a secondary service. Secondary services are included by other services
       to implement some higher-level functionality.
 */

/*! Creates a new invalid object of this class. */
QLowEnergyServiceData::QLowEnergyServiceData() : d(new QLowEnergyServiceDataPrivate)
{
}

/*! Constructs a new object of this class that is a copy of \a other. */
QLowEnergyServiceData::QLowEnergyServiceData(const QLowEnergyServiceData &other) : d(other.d)
{
}

/*! Destroys this object. */
QLowEnergyServiceData::~QLowEnergyServiceData()
{
}

/*! Makes this object a copy of \a other and returns the new value of this object. */
QLowEnergyServiceData &QLowEnergyServiceData::operator=(const QLowEnergyServiceData &other)
{
    d = other.d;
    return *this;
}

/*! Returns the type of this service. */
QLowEnergyServiceData::ServiceType QLowEnergyServiceData::type() const
{
    return d->type;
}

/*! Sets the type of this service to \a type. */
void QLowEnergyServiceData::setType(ServiceType type)
{
    d->type = type;
}

/*! Returns the UUID of this service. */
QBluetoothUuid QLowEnergyServiceData::uuid() const
{
    return d->uuid;
}

/*! Sets the UUID of this service to \a uuid. */
void QLowEnergyServiceData::setUuid(const QBluetoothUuid &uuid)
{
    d->uuid = uuid;
}

/*! Returns the list of included services. */
QList<QLowEnergyService *> QLowEnergyServiceData::includedServices() const
{
    return d->includedServices;
}

/*!
  Sets the list of included services to \a services.
  All objects in this list must have been returned from a call to
  \l QLowEnergyController::addService.
  \sa addIncludedService()
*/
void QLowEnergyServiceData::setIncludedServices(const QList<QLowEnergyService *> &services)
{
    d->includedServices = services;
}

/*!
  Adds \a service to the list of included services.
  The \a service object must have been returned from a call to
  \l QLowEnergyController::addService. This requirement prevents circular includes
  (which are forbidden by the Bluetooth specification), and also helps to support the use case of
  including more than one service of the same type.
  \sa setIncludedServices()
*/
void QLowEnergyServiceData::addIncludedService(QLowEnergyService *service)
{
    d->includedServices << service;
}

/*! Returns the list of characteristics. */
QList<QLowEnergyCharacteristicData> QLowEnergyServiceData::characteristics() const
{
    return d->characteristics;
}

/*!
  Sets the list of characteristics to \a characteristics.
  Only valid characteristics are considered.
  \sa addCharacteristic()
 */
void QLowEnergyServiceData::setCharacteristics(const QList<QLowEnergyCharacteristicData> &characteristics)
{
    d->characteristics.clear();
    foreach (const QLowEnergyCharacteristicData &cd, characteristics)
        addCharacteristic(cd);
}

/*!
  Adds \a characteristic to the list of characteristics, if it is valid.
  \sa setCharacteristics()
 */
void QLowEnergyServiceData::addCharacteristic(const QLowEnergyCharacteristicData &characteristic)
{
    if (characteristic.isValid())
        d->characteristics << characteristic;
    else
        qCWarning(QT_BT) << "not adding invalid characteristic to service";
}

/*! Returns \c true if this service is has a non-null UUID. */
bool QLowEnergyServiceData::isValid() const
{
    return !uuid().isNull();
}

/*!
   \fn void QLowEnergyServiceData::swap(QLowEnergyServiceData &other)
    Swaps this object with \a other.
 */

/*!
   Returns \c true if \a sd1 and \a sd2 are equal with respect to their public state,
   otherwise returns \c false.
 */
bool operator==(const QLowEnergyServiceData &sd1, const QLowEnergyServiceData &sd2)
{
    return sd1.d == sd2.d || (sd1.type() == sd2.type() && sd1.uuid() == sd2.uuid()
                              && sd1.includedServices() == sd2.includedServices()
                              && sd1.characteristics() == sd2.characteristics());
}

/*!
   \fn bool operator!=(const QLowEnergyServiceData &sd1,
                       const QLowEnergyServiceData &sd2)
   Returns \c true if \a sd1 and \a sd2 are not equal with respect to their public state,
   otherwise returns \c false.
 */

QT_END_NAMESPACE
