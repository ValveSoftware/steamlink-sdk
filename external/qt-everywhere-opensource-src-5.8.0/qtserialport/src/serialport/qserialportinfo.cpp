/****************************************************************************
**
** Copyright (C) 2011-2012 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2011 Sergey Belyashov <Sergey.Belyashov@gmail.com>
** Copyright (C) 2012 Laszlo Papp <lpapp@kde.org>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
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

#include "qserialportinfo.h"
#include "qserialportinfo_p.h"
#include "qserialport.h"
#include "qserialport_p.h"

QT_BEGIN_NAMESPACE


/*!
    \class QSerialPortInfo

    \brief Provides information about existing serial ports.

    \ingroup serialport-main
    \inmodule QtSerialPort
    \since 5.1

    Use the static functions to generate a list of QSerialPortInfo
    objects. Each QSerialPortInfo object in the list represents a single
    serial port and can be queried for the port name, system location,
    description, and manufacturer. The QSerialPortInfo class can also be
    used as an input parameter for the setPort() method of the QSerialPort
    class.

    \sa QSerialPort
*/

/*!
    Constructs an empty QSerialPortInfo object.

    \sa isNull()
*/
QSerialPortInfo::QSerialPortInfo()
{
}

/*!
    Constructs a copy of \a other.
*/
QSerialPortInfo::QSerialPortInfo(const QSerialPortInfo &other)
    : d_ptr(other.d_ptr ? new QSerialPortInfoPrivate(*other.d_ptr) : nullptr)
{
}

/*!
    Constructs a QSerialPortInfo object from serial \a port.
*/
QSerialPortInfo::QSerialPortInfo(const QSerialPort &port)
    : QSerialPortInfo(port.portName())
{
}

/*!
    Constructs a QSerialPortInfo object from serial port \a name.

    This constructor finds the relevant serial port among the available ones
    according to the port name \a name, and constructs the serial port info
    instance for that port.
*/
QSerialPortInfo::QSerialPortInfo(const QString &name)
{
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        if (name == info.portName()) {
            *this = info;
            break;
        }
    }
}

QSerialPortInfo::QSerialPortInfo(const QSerialPortInfoPrivate &dd)
    : d_ptr(new QSerialPortInfoPrivate(dd))
{
}

/*!
    Destroys the QSerialPortInfo object. References to the values in the
    object become invalid.
*/
QSerialPortInfo::~QSerialPortInfo()
{
}

/*! \fn void QSerialPortInfo::swap(QSerialPortInfo &other)

    Swaps QSerialPortInfo \a other with this QSerialPortInfo. This operation is
    very fast and never fails.
*/
void QSerialPortInfo::swap(QSerialPortInfo &other)
{
    d_ptr.swap(other.d_ptr);
}

/*!
    Sets the QSerialPortInfo object to be equal to \a other.
*/
QSerialPortInfo& QSerialPortInfo::operator=(const QSerialPortInfo &other)
{
    QSerialPortInfo(other).swap(*this);
    return *this;
}

/*!
    Returns the name of the serial port.

    \sa systemLocation()
*/
QString QSerialPortInfo::portName() const
{
    Q_D(const QSerialPortInfo);
    return !d ? QString() : d->portName;
}

/*!
    Returns the system location of the serial port.

    \sa portName()
*/
QString QSerialPortInfo::systemLocation() const
{
    Q_D(const QSerialPortInfo);
    return !d ? QString() : d->device;
}

/*!
    Returns the description string of the serial port,
    if available; otherwise returns an empty string.

    \sa manufacturer(), serialNumber()
*/
QString QSerialPortInfo::description() const
{
    Q_D(const QSerialPortInfo);
    return !d ? QString() : d->description;
}

/*!
    Returns the manufacturer string of the serial port,
    if available; otherwise returns an empty string.

    \sa description(), serialNumber()
*/
QString QSerialPortInfo::manufacturer() const
{
    Q_D(const QSerialPortInfo);
    return !d ? QString() : d->manufacturer;
}

/*!
    \since 5.3

    Returns the serial number string of the serial port,
    if available; otherwise returns an empty string.

    \note The serial number may include letters.

    \sa description(), manufacturer()
*/
QString QSerialPortInfo::serialNumber() const
{
    Q_D(const QSerialPortInfo);
    return !d ? QString() : d->serialNumber;
}

/*!
    Returns the 16-bit vendor number for the serial port, if available;
    otherwise returns zero.

    \sa hasVendorIdentifier(), productIdentifier(), hasProductIdentifier()
*/
quint16 QSerialPortInfo::vendorIdentifier() const
{
    Q_D(const QSerialPortInfo);
    return !d ? 0 : d->vendorIdentifier;
}

/*!
    Returns the 16-bit product number for the serial port, if available;
    otherwise returns zero.

    \sa hasProductIdentifier(), vendorIdentifier(), hasVendorIdentifier()
*/
quint16 QSerialPortInfo::productIdentifier() const
{
    Q_D(const QSerialPortInfo);
    return !d ? 0 : d->productIdentifier;
}

/*!
    Returns true if there is a valid 16-bit vendor number present; otherwise
    returns false.

    \sa vendorIdentifier(), productIdentifier(), hasProductIdentifier()
*/
bool QSerialPortInfo::hasVendorIdentifier() const
{
    Q_D(const QSerialPortInfo);
    return !d ? false : d->hasVendorIdentifier;
}

/*!
    Returns true if there is a valid 16-bit product number present; otherwise
    returns false.

    \sa productIdentifier(), vendorIdentifier(), hasVendorIdentifier()
*/
bool QSerialPortInfo::hasProductIdentifier() const
{
    Q_D(const QSerialPortInfo);
    return !d ? false : d->hasProductIdentifier;
}

/*!
    \fn bool QSerialPortInfo::isNull() const

    Returns whether this QSerialPortInfo object holds a
    serial port definition.

    \sa isBusy()
*/

#if QT_DEPRECATED_SINCE(5, 6)
/*!
    \fn bool QSerialPortInfo::isBusy() const

    Returns true if serial port is busy;
    otherwise returns false.

    \sa isNull()
*/
#endif // QT_DEPRECATED_SINCE(5, 6)

#if QT_DEPRECATED_SINCE(5, 2)
/*!
    \fn bool QSerialPortInfo::isValid() const
    \obsolete

    Returns true if serial port is present on system;
    otherwise returns false.

    \sa isNull(), isBusy()
*/
#endif // QT_DEPRECATED_SINCE(5, 2)

/*!
    \fn QList<qint32> QSerialPortInfo::standardBaudRates()

    Returns a list of available standard baud rates supported
    by the target platform.
*/
QList<qint32> QSerialPortInfo::standardBaudRates()
{
    return QSerialPortPrivate::standardBaudRates();
}

/*!
    \fn QList<QSerialPortInfo> QSerialPortInfo::availablePorts()

    Returns a list of available serial ports on the system.
*/

QT_END_NAMESPACE
