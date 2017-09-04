/****************************************************************************
**
** Copyright (C) 2017 Andre Hartmann <aha_1980@gmx.de>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialBus module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qcanbusdeviceinfo.h"
#include "qcanbusdeviceinfo_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QCanBusDeviceInfo
    \inmodule QtSerialBus
    \since 5.9

    \brief The QCanBusDeviceInfo provides information about CAN bus interfaces.

    Each plugin may support one or more interfaces with different
    capabilities. This class provides information about available functions.
*/

/*!
    Constructs a copy of \a other.
*/
QCanBusDeviceInfo::QCanBusDeviceInfo(const QCanBusDeviceInfo &) = default;

/*!
    Constructs a CAN bus device info from QCanBusDeviceInfoPrivate \a dd.
    \internal
*/
QCanBusDeviceInfo::QCanBusDeviceInfo(QCanBusDeviceInfoPrivate &dd) :
    d_ptr(&dd)
{
}

/*!
    Destroys the CAN bus device info.
*/
QCanBusDeviceInfo::~QCanBusDeviceInfo() = default;

/*!
    \fn void QCanBusDeviceInfo::swap(QCanBusDeviceInfo &other)
    Swaps this CAN bus device info with \a other. This operation is very fast
    and never fails.
*/

/*!
    \fn QCanBusDeviceInfo &QCanBusDeviceInfo::operator=(QCanBusDeviceInfo &&other)

    Move-assigns other to this QCanBusDeviceInfo instance.
*/

/*!
    Assigns \a other to this CAN bus device info and returns a reference to this
    CAN bus device info.
*/
QCanBusDeviceInfo &QCanBusDeviceInfo::operator=(const QCanBusDeviceInfo &) = default;

/*!
    Returns the interface name of this CAN bus interface, e.g. can0.
*/
QString QCanBusDeviceInfo::name() const
{
    return d_ptr->name;
}

/*!
    Returns true, if the CAN bus interface is CAN FD (flexible data rate) capable.

    If this information is not available, false is returned.
*/
bool QCanBusDeviceInfo::hasFlexibleDataRate() const
{
    return d_ptr->hasFlexibleDataRate;
}

/*!
    Returns true, if the CAN bus interface is virtual (i.e. not connected to real
    CAN hardware).

    If this information is not available, false is returned.
*/
bool QCanBusDeviceInfo::isVirtual() const
{
    return d_ptr->isVirtual;
}

QT_END_NAMESPACE
