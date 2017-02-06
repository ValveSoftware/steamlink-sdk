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

#include "qmodbusclient.h"
#include "qmodbusclient_p.h"
#include "qmodbus_symbols_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qloggingcategory.h>

#include <bitset>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_MODBUS)

/*!
    \class QModbusClient
    \inmodule QtSerialBus
    \since 5.8

    \brief The QModbusClient class is the interface to send Modbus requests.

    The QModbusClient API is constructed around one QModbusClient object, which holds the common
    configuration and settings for the requests it sends. One QModbusClient should be enough for
    the whole Qt application.

    Once a QModbusClient object has been created, the application can use it to send requests.
    The returned object is used to obtain any data returned in response to the corresponding request.

    QModbusClient has an asynchronous API. When the finished slot is called, the parameter
    it takes is the QModbusReply object containing the PDU as well as meta-data (Addressing, etc.).

    Note: QModbusClient queues the requests it receives. The number of requests executed in
    parallel is dependent on the protocol. For example, the HTTP protocol on desktop platforms
    issues 6 requests in parallel for one host/port combination.
*/

/*!
    Constructs a Modbus client device with the specified \a parent.
*/
QModbusClient::QModbusClient(QObject *parent)
    : QModbusDevice(*new QModbusClientPrivate, parent)
{
}

/*!
    \internal
*/
QModbusClient::~QModbusClient()
{
}

/*!
    Sends a request to read the contents of the data pointed by \a read.
    Returns a new valid \l QModbusReply object if no error occurred, otherwise
    nullptr. Modbus network may have multiple servers, each server has unique
    \a serverAddress.
*/
QModbusReply *QModbusClient::sendReadRequest(const QModbusDataUnit &read, int serverAddress)
{
    Q_D(QModbusClient);
    return d->sendRequest(d->createReadRequest(read), serverAddress, &read);
}

/*!
    Sends a request to modify the contents of the data pointed by \a write.
    Returns a new valid \l QModbusReply object if no error occurred, otherwise
    nullptr. Modbus network may have multiple servers, each server has unique
    \a serverAddress.
*/
QModbusReply *QModbusClient::sendWriteRequest(const QModbusDataUnit &write, int serverAddress)
{
    Q_D(QModbusClient);
    return d->sendRequest(d->createWriteRequest(write), serverAddress, &write);
}

/*!
    Sends a request to read the contents of the data pointed by \a read and to
    modify the contents of the data pointed by \a write using Modbus function
    code \l QModbusPdu::ReadWriteMultipleRegisters.
    Returns a new valid \l QModbusReply object if no error occurred, otherwise
    nullptr. Modbus network may have multiple servers, each server has unique
    \a serverAddress.

    \note: Sending this kind of request is only valid of both \a read and
    \a write are of type QModbusDataUnit::HoldingRegisters.
*/
QModbusReply *QModbusClient::sendReadWriteRequest(const QModbusDataUnit &read,
                                                  const QModbusDataUnit &write, int serverAddress)
{
    Q_D(QModbusClient);
    return d->sendRequest(d->createRWRequest(read, write), serverAddress, &read);
}

/*!
    Sends a raw Modbus \a request. A raw request can contain anything that
    fits inside the Modbus PDU data section and has a valid function code.
    The only check performed before sending is therefore the validity check,
    see \l QModbusPdu::isValid. If no error occurred the function returns a
    a new valid \l QModbusReply; nullptr otherwise. Modbus networks may have
    multiple servers, each server has a unique \a serverAddress.

    \sa QModbusReply::rawResult()
*/
QModbusReply *QModbusClient::sendRawRequest(const QModbusRequest &request, int serverAddress)
{
    return d_func()->sendRequest(request, serverAddress, nullptr);
}

/*!
    \property QModbusClient::timeout
    \brief the timeout value used by this client

    Returns the timeout value used by this QModbusClient instance in ms.
    A timeout is indicated by a \l TimeoutError. The default value is 1000 ms.

    \sa setTimeout
*/
int QModbusClient::timeout() const
{
    Q_D(const QModbusClient);
    return d->m_responseTimeoutDuration;
}

/*!
    \fn void QModbusClient::timeoutChanged(int newTimeout)

    This signal is emitted if the response is not received within the required
    timeout. The new response timeout for the device is passed as \a newTimeout.
*/

/*!
    Sets the \a newTimeout for this QModbusClient instance. The minimum timeout
    is 50 ms.

    The timeout is used by the client to determine how long it waits for
    a response from the server. If the response is not received within the
    required timeout, the \l TimeoutError is set.

    Already active/running timeouts are not affected by such timeout duration
    changes.

    \sa timeout
*/
void QModbusClient::setTimeout(int newTimeout)
{
    if (newTimeout < 50)
        return;

    Q_D(QModbusClient);
    if (d->m_responseTimeoutDuration != newTimeout) {
        d->m_responseTimeoutDuration = newTimeout;
        emit timeoutChanged(newTimeout);
    }
}

/*!
    Returns the number of retries a client will perform before a
    request fails. The default value is set to \c 3.
*/
int QModbusClient::numberOfRetries() const
{
    Q_D(const QModbusClient);
    return d->m_numberOfRetries;
}

/*!
    Sets the \a number of retries a client will perform before a
    request fails. The default value is set to \c 3.

    \note The new value must be greater than or equal to \c 0. Changing this
    property will only effect new requests, not already scheduled ones.
*/
void QModbusClient::setNumberOfRetries(int number)
{
    Q_D(QModbusClient);
    if (number >= 0)
        d->m_numberOfRetries = number;
}

/*!
    \internal
*/
QModbusClient::QModbusClient(QModbusClientPrivate &dd, QObject *parent) :
    QModbusDevice(dd, parent)
{

}

/*!
    Processes a Modbus server \a response and stores the decoded information in \a data. Returns
    true on success; otherwise false.
*/
bool QModbusClient::processResponse(const QModbusResponse &response, QModbusDataUnit *data)
{
    return d_func()->processResponse(response, data);
}

/*!
    To be implemented by custom Modbus client implementation. The default implementation ignores
    \a response and \a data. It always returns false to indicate error.
*/
bool QModbusClient::processPrivateResponse(const QModbusResponse &response, QModbusDataUnit *data)
{
    Q_UNUSED(response)
    Q_UNUSED(data)
    return false;
}

QModbusReply *QModbusClientPrivate::sendRequest(const QModbusRequest &request, int serverAddress,
                                                const QModbusDataUnit *const unit)
{
    Q_Q(QModbusClient);

    if (!isOpen() || q->state() != QModbusDevice::ConnectedState) {
        qCWarning(QT_MODBUS) << "(Client) Device is not connected";
        q->setError(QModbusClient::tr("Device not connected."), QModbusDevice::ConnectionError);
        return nullptr;
    }

    if (!request.isValid()) {
        qCWarning(QT_MODBUS) << "(Client) Refuse to send invalid request.";
        q->setError(QModbusClient::tr("Invalid Modbus request."), QModbusDevice::ProtocolError);
        return nullptr;
    }

    if (unit)
        return enqueueRequest(request, serverAddress, *unit, QModbusReply::Common);
    return enqueueRequest(request, serverAddress, QModbusDataUnit(), QModbusReply::Raw);
}

QModbusRequest QModbusClientPrivate::createReadRequest(const QModbusDataUnit &data) const
{
    if (!data.isValid())
        return QModbusRequest();

    switch (data.registerType()) {
    case QModbusDataUnit::Coils:
        return QModbusRequest(QModbusRequest::ReadCoils, quint16(data.startAddress()),
                              quint16(data.valueCount()));
    case QModbusDataUnit::DiscreteInputs:
        return QModbusRequest(QModbusRequest::ReadDiscreteInputs, quint16(data.startAddress()),
                              quint16(data.valueCount()));
    case QModbusDataUnit::InputRegisters:
        return QModbusRequest(QModbusRequest::ReadInputRegisters, quint16(data.startAddress()),
                              quint16(data.valueCount()));
    case QModbusDataUnit::HoldingRegisters:
        return QModbusRequest(QModbusRequest::ReadHoldingRegisters, quint16(data.startAddress()),
                              quint16(data.valueCount()));
    default:
        break;
    }

    return QModbusRequest();
}

QModbusRequest QModbusClientPrivate::createWriteRequest(const QModbusDataUnit &data) const
{
    switch (data.registerType()) {
    case QModbusDataUnit::Coils: {
        if (data.valueCount() == 1) {
            return QModbusRequest(QModbusRequest::WriteSingleCoil, quint16(data.startAddress()),
                                  quint16((data.value(0) == 0u) ? Coil::Off : Coil::On));
        }

        quint8 byteCount = data.valueCount() / 8;
        if ((data.valueCount() % 8) != 0)
            byteCount += 1;

        quint8 address = 0;
        QVector<quint8> bytes;
        for (quint8 i = 0; i < byteCount; ++i) {
            std::bitset<8> byte;
            for (int currentBit = 0; currentBit < 8; ++currentBit)
                byte[currentBit] = data.value(address++);
            bytes.append(static_cast<quint8> (byte.to_ulong()));
        }

        return QModbusRequest(QModbusRequest::WriteMultipleCoils, quint16(data.startAddress()),
                              quint16(data.valueCount()), byteCount, bytes);
    }   break;

    case QModbusDataUnit::HoldingRegisters: {
        if (data.valueCount() == 1) {
            return QModbusRequest(QModbusRequest::WriteSingleRegister, quint16(data.startAddress()),
                                  data.value(0));
        }

        const quint8 byteCount = data.valueCount() * 2;
        return QModbusRequest(QModbusRequest::WriteMultipleRegisters, quint16(data.startAddress()),
                              quint16(data.valueCount()), byteCount, data.values());
    }   break;

    case QModbusDataUnit::DiscreteInputs:
    case QModbusDataUnit::InputRegisters:
    default:    // fall through on purpose
        break;
    }
    return QModbusRequest();
}

QModbusRequest QModbusClientPrivate::createRWRequest(const QModbusDataUnit &read,
                                                     const QModbusDataUnit &write) const
{
    if ((read.registerType() != QModbusDataUnit::HoldingRegisters)
        && (write.registerType() != QModbusDataUnit::HoldingRegisters)) {
        return QModbusRequest();
    }

    const quint8 byteCount = write.valueCount() * 2;
    return QModbusRequest(QModbusRequest::ReadWriteMultipleRegisters, quint16(read.startAddress()),
                          quint16(read.valueCount()), quint16(write.startAddress()),
                          quint16(write.valueCount()), byteCount, write.values());
}

void QModbusClientPrivate::processQueueElement(const QModbusResponse &pdu,
                                               const QueueElement &element)
{
    element.reply->setRawResult(pdu);
    if (pdu.isException()) {
        element.reply->setError(QModbusDevice::ProtocolError,
            QModbusClient::tr("Modbus Exception Response."));
        return;
    }

    if (element.reply->type() == QModbusReply::Raw) {
        element.reply->setFinished(true);
        return;
    }

    QModbusDataUnit unit = element.unit;
    if (!processResponse(pdu, &unit)) {
        element.reply->setError(QModbusDevice::UnknownError,
            QModbusClient::tr("An invalid response has been received."));
        return;
    }

    element.reply->setResult(unit);
    element.reply->setFinished(true);
}

bool QModbusClientPrivate::processResponse(const QModbusResponse &response, QModbusDataUnit *data)
{
    switch (response.functionCode()) {
    case QModbusRequest::ReadCoils:
        return processReadCoilsResponse(response, data);
    case QModbusRequest::ReadDiscreteInputs:
        return processReadDiscreteInputsResponse(response, data);
    case QModbusRequest::ReadHoldingRegisters:
        return processReadHoldingRegistersResponse(response, data);
    case QModbusRequest::ReadInputRegisters:
        return processReadInputRegistersResponse(response, data);
    case QModbusRequest::WriteSingleCoil:
        return processWriteSingleCoilResponse(response, data);
    case QModbusRequest::WriteSingleRegister:
        return processWriteSingleRegisterResponse(response, data);
    case QModbusRequest::ReadExceptionStatus:
    case QModbusRequest::Diagnostics:
    case QModbusRequest::GetCommEventCounter:
    case QModbusRequest::GetCommEventLog:
        return false;   // Return early, it's not a private response.
    case QModbusRequest::WriteMultipleCoils:
        return processWriteMultipleCoilsResponse(response, data);
    case QModbusRequest::WriteMultipleRegisters:
        return processWriteMultipleRegistersResponse(response, data);
    case QModbusRequest::ReportServerId:
    case QModbusRequest::ReadFileRecord:
    case QModbusRequest::WriteFileRecord:
    case QModbusRequest::MaskWriteRegister:
        return false;   // Return early, it's not a private response.
    case QModbusRequest::ReadWriteMultipleRegisters:
        return processReadWriteMultipleRegistersResponse(response, data);
    case QModbusRequest::ReadFifoQueue:
    case QModbusRequest::EncapsulatedInterfaceTransport:
        return false;   // Return early, it's not a private response.
    default:
        break;
    }
    return q_func()->processPrivateResponse(response, data);
}

static bool isValid(const QModbusResponse &response, QModbusResponse::FunctionCode fc)
{
    if (!response.isValid())
        return false;
    if (response.isException())
        return false;
    if (response.functionCode() != fc)
        return false;
    return true;
}

bool QModbusClientPrivate::processReadCoilsResponse(const QModbusResponse &response,
                                                    QModbusDataUnit *data)
{
    if (!isValid(response, QModbusResponse::ReadCoils))
        return false;
    return collateBits(response, QModbusDataUnit::Coils, data);
}

bool QModbusClientPrivate::processReadDiscreteInputsResponse(const QModbusResponse &response,
                                                             QModbusDataUnit *data)
{
    if (!isValid(response, QModbusResponse::ReadDiscreteInputs))
        return false;
    return collateBits(response, QModbusDataUnit::DiscreteInputs, data);
}

bool QModbusClientPrivate::collateBits(const QModbusPdu &response,
                                     QModbusDataUnit::RegisterType type, QModbusDataUnit *data)
{
    if (response.dataSize() < QModbusResponse::minimumDataSize(response))
        return false;

    const QByteArray payload = response.data();
    // byte count needs to match available bytes
    if ((payload.size() - 1) != payload[0])
        return false;

    if (data) {
        uint value = 0;
        for (qint32 i = 1; i < payload.size(); ++i) {
            const std::bitset<8> byte = payload[i];
            for (qint32 currentBit = 0; currentBit < 8 && value < data->valueCount(); ++currentBit)
                data->setValue(value++, byte[currentBit]);
        }
        data->setRegisterType(type);
    }
    return true;
}

bool QModbusClientPrivate::processReadHoldingRegistersResponse(const QModbusResponse &response,
                                                               QModbusDataUnit *data)
{
    if (!isValid(response, QModbusResponse::ReadHoldingRegisters))
        return false;
    return collateBytes(response, QModbusDataUnit::HoldingRegisters, data);
}

bool QModbusClientPrivate::processReadInputRegistersResponse(const QModbusResponse &response,
                                                             QModbusDataUnit *data)
{
    if (!isValid(response, QModbusResponse::ReadInputRegisters))
        return false;
    return collateBytes(response, QModbusDataUnit::InputRegisters, data);
}

bool QModbusClientPrivate::collateBytes(const QModbusPdu &response,
                                      QModbusDataUnit::RegisterType type, QModbusDataUnit *data)
{
    if (response.dataSize() < QModbusResponse::minimumDataSize(response))
        return false;

    // byte count needs to match available bytes
    const quint8 byteCount = response.data()[0];
    if ((response.dataSize() - 1) != byteCount)
        return false;

    // byte count needs to be odd to match full registers
    if (byteCount % 2 != 0)
        return false;

    if (data) {
        QDataStream stream(response.data().remove(0, 1));

        QVector<quint16> values;
        const quint8 itemCount = byteCount / 2;
        for (int i = 0; i < itemCount; i++) {
            quint16 tmp;
            stream >> tmp;
            values.append(tmp);
        }
        data->setValues(values);
        data->setRegisterType(type);
    }
    return true;
}

bool QModbusClientPrivate::processWriteSingleCoilResponse(const QModbusResponse &response,
    QModbusDataUnit *data)
{
    if (!isValid(response, QModbusResponse::WriteSingleCoil))
        return false;
    return collateSingleValue(response, QModbusDataUnit::Coils, data);
}

bool QModbusClientPrivate::processWriteSingleRegisterResponse(const QModbusResponse &response,
    QModbusDataUnit *data)
{
    if (!isValid(response, QModbusResponse::WriteSingleRegister))
        return false;
    return collateSingleValue(response, QModbusDataUnit::HoldingRegisters, data);
}

bool QModbusClientPrivate::collateSingleValue(const QModbusPdu &response,
                                       QModbusDataUnit::RegisterType type, QModbusDataUnit *data)
{
    if (response.dataSize() != QModbusResponse::minimumDataSize(response))
        return false;

    quint16 address, value;
    response.decodeData(&address, &value);
    if ((type == QModbusDataUnit::Coils) && (value != Coil::Off) && (value != Coil::On))
        return false;

    if (data) {
        data->setRegisterType(type);
        data->setStartAddress(address);
        data->setValues(QVector<quint16>{ value });
    }
    return true;
}

bool QModbusClientPrivate::processWriteMultipleCoilsResponse(const QModbusResponse &response,
                                                             QModbusDataUnit *data)
{
    if (!isValid(response, QModbusResponse::WriteMultipleCoils))
        return false;
    return collateMultipleValues(response, QModbusDataUnit::Coils, data);
}

bool QModbusClientPrivate::processWriteMultipleRegistersResponse(const QModbusResponse &response,
                                                                 QModbusDataUnit *data)
{
    if (!isValid(response, QModbusResponse::WriteMultipleRegisters))
        return false;
    return collateMultipleValues(response, QModbusDataUnit::HoldingRegisters, data);
}

bool QModbusClientPrivate::collateMultipleValues(const QModbusPdu &response,
                                      QModbusDataUnit::RegisterType type, QModbusDataUnit *data)
{
    if (response.dataSize() != QModbusResponse::minimumDataSize(response))
        return false;

    quint16 address, count;
    response.decodeData(&address, &count);

    // number of registers to write is 1-123 per request
    if ((type == QModbusDataUnit::HoldingRegisters) && (count < 1 || count > 123))
        return false;

    if (data) {
        data->setValueCount(count);
        data->setRegisterType(type);
        data->setStartAddress(address);
    }
    return true;
}

bool QModbusClientPrivate::processReadWriteMultipleRegistersResponse(const QModbusResponse &resp,
                                                                     QModbusDataUnit *data)
{
    if (!isValid(resp, QModbusResponse::ReadWriteMultipleRegisters))
        return false;
    return collateBytes(resp, QModbusDataUnit::HoldingRegisters, data);
}

QT_END_NAMESPACE
