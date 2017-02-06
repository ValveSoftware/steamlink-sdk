/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#include "qmodbusdevice.h"
#include "qmodbusdevice_p.h"
#include "qmodbusdataunit.h"

#include <QtCore/qloggingcategory.h>

QT_BEGIN_NAMESPACE

/*!
    \class QModbusDevice
    \inmodule QtSerialBus
    \since 5.8

    \brief The QModbusDevice class is the base class for Modbus classes, \l QModbusServer
    and \l QModbusClient.
*/

/*!
    Constructs a Modbus device with the specified \a parent.
*/
QModbusDevice::QModbusDevice(QObject *parent)
 : QObject(*new QModbusDevicePrivate, parent)
{
}

/*!
    \internal
*/
QModbusDevice::QModbusDevice(QModbusDevicePrivate &dd, QObject *parent)
 : QObject(dd, parent)
{
}

/*!
    Destroys the QModbusDevice instance
*/
QModbusDevice::~QModbusDevice()
{
}

/*!
    \enum QModbusDevice::ConnectionParameter

    This enum describes the possible values that can be set for a Modbus device
    connection.

    The general purpose value (and the associated types) are:

    \value SerialPortNameParameter   This parameter holds the serial port used for
                                     device communication, e.g. COM1. \c QString
    \value SerialParityParameter     This parameter holds the parity checking mode.
                                     \c QSerialPort::Parity
    \value SerialBaudRateParameter   This parameter holds the data baud rate for
                                     the communication. \c QSerialPort::BaudRate
    \value SerialDataBitsParameter   This parameter holds the data bits in a frame.
                                     \c QSerialPort::DataBits
    \value SerialStopBitsParameter   This parameter holds the number of stop bits in a
                                     frame. \c QSerialPort::StopBits
    \value NetworkPortParameter      This parameter holds the network port. \c int
    \value NetworkAddressParameter   This parameter holds the host address for network
                                     communication. \c QString

    User options:

    \value UserParameter             The first parameter that can be used for user-specific
                                     purposes. \c QVariant
*/

/*!
    Returns the value associated with the given connection \a parameter. The
    returned value can be empty.

    By default the \c QModbusDevice is initialized with some common values. The
    serial port settings are even parity, a baud rate of 19200 bits per second,
    eight data bits and one stop bit. The network settings for the host address
    is set to local host and port to 502.

    \note For a serial connection to succeed, the \l SerialPortNameParameter
    needs to be set to a valid communication port. The information about valid
    serial ports can be obtained from \l QSerialPortInfo.

    \note If the device is already connected, the settings are taken into account
    after reconnecting the device.

    \sa ConnectionParameter
*/
QVariant QModbusDevice::connectionParameter(int parameter) const
{
    Q_D(const QModbusDevice);
    switch (parameter) {
    case SerialPortNameParameter:
        return d->m_comPort;
    case SerialDataBitsParameter:
        return d->m_dataBits;
    case SerialParityParameter:
        return d->m_parity;
    case SerialStopBitsParameter:
        return d->m_stopBits;
    case SerialBaudRateParameter:
        return d->m_baudRate;
    case NetworkPortParameter:
        return d->m_networkPort;
    case NetworkAddressParameter:
        return d->m_networkAddress;
    default:
        break;
    }
    return d->m_userConnectionParams.value(parameter);
}

/*!
    Sets the value of \a parameter to \a value. If the \a parameter already
    exists, the previous value is overwritten. A active or running connection
    is not affected by such parameter changes.

    \sa ConnectionParameter
    \sa connectionParameter()
*/
void QModbusDevice::setConnectionParameter(int parameter, const QVariant &value)
{
    Q_D(QModbusDevice);
    switch (parameter) {
    case SerialPortNameParameter:
        d->m_comPort = value.toString();
        break;
    case SerialDataBitsParameter:
        d->m_dataBits = QSerialPort::DataBits(value.toInt());
        break;
    case SerialParityParameter:
        d->m_parity = QSerialPort::Parity(value.toInt());
        break;
    case SerialStopBitsParameter:
        d->m_stopBits = QSerialPort::StopBits(value.toInt());
        break;
    case SerialBaudRateParameter:
        d->m_baudRate = QSerialPort::BaudRate(value.toInt());
        break;
    case NetworkPortParameter:
        d->m_networkPort = value.toInt();
        break;
    case NetworkAddressParameter:
        d->m_networkAddress = value.toString();
        break;
    default:
        d->m_userConnectionParams.insert(parameter, value);
        break;
    }
}

/*!
    \enum QModbusDevice::Error
    This enum describes all the possible error conditions.

    \value NoError              No errors have occurred.
    \value ReadError            An error occurred during a read operation.
    \value WriteError           An error occurred during a write operation.
    \value ConnectionError      An error occurred when attempting to open the
                                backend.
    \value ConfigurationError   An error occurred when attempting to set a
                                configuration parameter.
    \value TimeoutError         A timeout occurred during I/O. An I/O operation
                                did not finish within a given time frame.
    \value ProtocolError        A Modbus specific protocol error occurred.
    \value ReplyAbortedError    The reply was aborted due to a disconnection of
                                the device.
    \value UnknownError         An unknown error occurred.
*/

/*!
    \enum QModbusDevice::State
    This enum describes all possible device states.

    \value UnconnectedState The device is disconnected.
    \value ConnectingState  The device is being connected.
    \value ConnectedState   The device is connected to the Modbus network.
    \value ClosingState     The device is being closed.
*/

/*!
    \fn QModbusDevice::errorOccurred(QModbusDevice::Error error)

    This signal is emitted when an error of the type, \a error, occurs.
*/

/*!
    \fn void QModbusDevice::stateChanged(QModbusDevice::State state)

    This signal is emitted every time the state of the device changes.
    The new state is represented by \a state.

    \sa setState(), state()
*/

/*!
    Connects the device to the Modbus network. Returns \c true on success;
    otherwise \c false.

    This function calls \l open() as part of its implementation.
*/
bool QModbusDevice::connectDevice()
{
    Q_D(QModbusDevice);

    if (d->state != QModbusDevice::UnconnectedState)
        return false;

    setState(ConnectingState);

    if (!open()) {
        setState(UnconnectedState);
        return false;
    }

    //Connected is set by backend -> might be delayed by event loop
    return true;
}

/*!
    Disconnects the device.

    This function calls \l close() as part of its implementation.
*/
void QModbusDevice::disconnectDevice()
{
    setState(QModbusDevice::ClosingState);

    //Unconnected is set by backend -> might be delayed by event loop
    close();
}

/*!
    Sets the state of the device to \a newState. Modbus device implementations
    must use this function to update the device state.
*/
void QModbusDevice::setState(QModbusDevice::State newState)
{
    Q_D(QModbusDevice);

    if (newState == d->state)
        return;

    d->state = newState;
    emit stateChanged(newState);
}

/*!
    Returns the current state of the device.

    \sa setState(), stateChanged()
*/
QModbusDevice::State QModbusDevice::state() const
{
    return d_func()->state;
}

/*!
    Sets the error state of the device. ModBus device implementations
    must use this function in case of an error to set the \a error type and
    a descriptive \a errorText.

    \sa QModbusDevice::Error
*/
void QModbusDevice::setError(const QString &errorText, QModbusDevice::Error error)
{
    Q_D(QModbusDevice);

    d->error = error;
    d->errorString = errorText;
    emit errorOccurred(error);
}

/*!
    Returns the error state of the device.

    \sa QModbusDevice::Error
*/
QModbusDevice::Error QModbusDevice::error() const
{
    return d_func()->error;
}

/*!
    Returns descriptive error text for the device error.

    \sa QModbusDevice::Error
*/
QString QModbusDevice::errorString() const
{
    return d_func()->errorString;
}

/*!
    \fn bool QModbusDevice::open()

    This function is called by connectDevice(). Subclasses must provide
    an implementation that returns \c true on successful Modbus connection
    or \c false otherwise.

    The implementation must ensure that the instance's \l state()
    is set to \l QModbusDevice::ConnectedState upon success; otherwise
    \l QModbusDevice::UnconnectedState.

    \sa connectDevice()
*/

/*!
    \fn void QModbusDevice::close()

    This function is responsible for closing the Modbus connection.
    The implementation must ensure that the instance's
    \l state() is set to \l QModbusDevice::UnconnectedState.

    \sa disconnectDevice()
*/

Q_LOGGING_CATEGORY(QT_MODBUS, "qt.modbus")
Q_LOGGING_CATEGORY(QT_MODBUS_LOW, "qt.modbus.lowlevel")

QT_END_NAMESPACE
