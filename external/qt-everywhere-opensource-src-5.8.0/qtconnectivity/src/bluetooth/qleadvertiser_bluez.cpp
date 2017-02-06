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

#include "qleadvertiser_p.h"

#include "bluez/bluez_data_p.h"
#include "bluez/hcimanager_p.h"
#include "qbluetoothsocket_p.h"

#include <QtCore/qloggingcategory.h>

#include <cstring>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_BLUEZ)

struct AdvParams {
    quint16 minInterval;
    quint16 maxInterval;
    quint8 type;
    quint8 ownAddrType;
    quint8 directAddrType;
    bdaddr_t directAddr;
    quint8 channelMap;
    quint8 filterPolicy;
} __attribute__ ((packed));

struct AdvData {
    quint8 length;
    quint8 data[31];
};

struct WhiteListParams {
    quint8 addrType;
    bdaddr_t addr;
};


template<typename T> QByteArray byteArrayFromStruct(const T &data, int maxSize = -1)
{
    return QByteArray(reinterpret_cast<const char *>(&data), maxSize != -1 ? maxSize : sizeof data);
}

QLeAdvertiserBluez::QLeAdvertiserBluez(const QLowEnergyAdvertisingParameters &params,
                                       const QLowEnergyAdvertisingData &advertisingData,
                                       const QLowEnergyAdvertisingData &scanResponseData,
                                       HciManager &hciManager, QObject *parent)
    : QLeAdvertiser(params, advertisingData, scanResponseData, parent), m_hciManager(hciManager)
{
    connect(&m_hciManager, &HciManager::commandCompleted, this,
            &QLeAdvertiserBluez::handleCommandCompleted);
}

QLeAdvertiserBluez::~QLeAdvertiserBluez()
{
    disconnect(&m_hciManager, &HciManager::commandCompleted, this,
               &QLeAdvertiserBluez::handleCommandCompleted);
    doStopAdvertising();
}

void QLeAdvertiserBluez::doStartAdvertising()
{
    if (!m_hciManager.monitorEvent(HciManager::CommandCompleteEvent)) {
        handleError();
        return;
    }

    m_disableCommandFinished = false;
    m_sendPowerLevel = advertisingData().includePowerLevel()
            || scanResponseData().includePowerLevel();
    if (m_sendPowerLevel)
        queueReadTxPowerLevelCommand();
    else
        queueAdvertisingCommands();
    sendNextCommand();
}

void QLeAdvertiserBluez::doStopAdvertising()
{
    toggleAdvertising(false);
}

void QLeAdvertiserBluez::queueCommand(OpCodeCommandField ocf, const QByteArray &data)
{
    m_pendingCommands << Command(ocf, data);
}

void QLeAdvertiserBluez::sendNextCommand()
{
    if (m_pendingCommands.isEmpty()) {
        // TODO: Unmonitor event.
        return;
    }
    const Command &c = m_pendingCommands.first();
    if (!m_hciManager.sendCommand(OgfLinkControl, c.ocf, c.data)) {
        handleError();
        return;
    }
}

void QLeAdvertiserBluez::queueAdvertisingCommands()
{
    toggleAdvertising(false); // Stop advertising first, in case it's currently active.
    setWhiteList();
    setAdvertisingParams();
    setAdvertisingData();
    setScanResponseData();
    toggleAdvertising(true);
}

void QLeAdvertiserBluez::queueReadTxPowerLevelCommand()
{
    // Spec v4.2, Vol 2, Part E, 7.8.6
    queueCommand(OcfLeReadTxPowerLevel, QByteArray());
}

void QLeAdvertiserBluez::toggleAdvertising(bool enable)
{
    // Spec v4.2, Vol 2, Part E, 7.8.9
    queueCommand(OcfLeSetAdvEnable, QByteArray(1, enable));
}

void QLeAdvertiserBluez::setAdvertisingParams()
{
    // Spec v4.2, Vol 2, Part E, 7.8.5
    AdvParams params;
    static_assert(sizeof params == 15, "unexpected struct size");
    using namespace std;
    memset(&params, 0, sizeof params);
    setAdvertisingInterval(params);
    params.type = parameters().mode();
    params.filterPolicy = parameters().filterPolicy();
    if (params.filterPolicy != QLowEnergyAdvertisingParameters::IgnoreWhiteList
            && advertisingData().discoverability() == QLowEnergyAdvertisingData::DiscoverabilityLimited) {
        qCWarning(QT_BT_BLUEZ) << "limited discoverability is incompatible with "
                                  "using a white list; disabling filtering";
        params.filterPolicy = QLowEnergyAdvertisingParameters::IgnoreWhiteList;
    }
    params.ownAddrType = QLowEnergyController::PublicAddress; // TODO: Make configurable.

    // TODO: For ADV_DIRECT_IND.
    // params.directAddrType = xxx;
    // params.direct_bdaddr = xxx;

    params.channelMap = 0x7; // All channels.

    const QByteArray paramsData = byteArrayFromStruct(params);
    qCDebug(QT_BT_BLUEZ) << "advertising parameters:" << paramsData.toHex();
    queueCommand(OcfLeSetAdvParams, paramsData);
}

static quint16 forceIntoRange(quint16 val, quint16 min, quint16 max)
{
    return qMin(qMax(val, min), max);
}

void QLeAdvertiserBluez::setAdvertisingInterval(AdvParams &params)
{
    const double multiplier = 0.625;
    const quint16 minVal = parameters().minimumInterval() / multiplier;
    const quint16 maxVal = parameters().maximumInterval() / multiplier;
    Q_ASSERT(minVal <= maxVal);
    const quint16 specMinimum =
            parameters().mode() == QLowEnergyAdvertisingParameters::AdvScanInd
            || parameters().mode() == QLowEnergyAdvertisingParameters::AdvNonConnInd ? 0xa0 : 0x20;
    const quint16 specMaximum = 0x4000;
    params.minInterval = qToLittleEndian(forceIntoRange(minVal, specMinimum, specMaximum));
    params.maxInterval = qToLittleEndian(forceIntoRange(maxVal, specMinimum, specMaximum));
    Q_ASSERT(params.minInterval <= params.maxInterval);
}

void QLeAdvertiserBluez::setPowerLevel(AdvData &advData)
{
    if (m_sendPowerLevel) {
        advData.data[advData.length++] = 2;
        advData.data[advData.length++]= 0xa;
        advData.data[advData.length++] = m_powerLevel;
    }
}

void QLeAdvertiserBluez::setFlags(AdvData &advData)
{
    // TODO: Discoverability flags are incompatible with ADV_DIRECT_IND
    quint8 flags = 0;
    if (advertisingData().discoverability() == QLowEnergyAdvertisingData::DiscoverabilityLimited)
        flags |= 0x1;
    else if (advertisingData().discoverability() == QLowEnergyAdvertisingData::DiscoverabilityGeneral)
        flags |= 0x2;
    flags |= 0x4; // "BR/EDR not supported". Otherwise clients might try to connect over Bluetooth classic.
    if (flags) {
        advData.data[advData.length++] = 2;
        advData.data[advData.length++] = 0x1;
        advData.data[advData.length++] = flags;
    }
}

template<typename T> static quint8 servicesType(bool dataComplete);
template<> quint8 servicesType<quint16>(bool dataComplete)
{
    return dataComplete ? 0x3 : 0x2;
}
template<> quint8 servicesType<quint32>(bool dataComplete)
{
    return dataComplete ? 0x5 : 0x4;
}
template<> quint8 servicesType<quint128>(bool dataComplete)
{
    return dataComplete ? 0x7 : 0x6;
}

template<typename T> static void addServicesData(AdvData &data, const QVector<T> &services)
{
    if (services.isEmpty())
        return;
    const int spaceAvailable = sizeof data.data - data.length;
    const int maxServices = qMin<int>((spaceAvailable - 2) / sizeof(T), services.count());
    if (maxServices <= 0) {
        qCWarning(QT_BT_BLUEZ) << "services data does not fit into advertising data packet";
        return;
    }
    const bool dataComplete = maxServices == services.count();
    if (!dataComplete) {
        qCWarning(QT_BT_BLUEZ) << "only" << maxServices << "out of" << services.count()
                               << "services fit into the advertising data";
    }
    data.data[data.length++] = 1 + maxServices * sizeof(T);
    data.data[data.length++] = servicesType<T>(dataComplete);
    for (int i = 0; i < maxServices; ++i) {
        putBtData(services.at(i), data.data + data.length);
        data.length += sizeof(T);
    }
}

void QLeAdvertiserBluez::setServicesData(const QLowEnergyAdvertisingData &src, AdvData &dest)
{
    QVector<quint16> services16;
    QVector<quint32> services32;
    QVector<quint128> services128;
    foreach (const QBluetoothUuid &service, src.services()) {
        bool ok;
        const quint16 service16 = service.toUInt16(&ok);
        if (ok) {
            services16 << service16;
            continue;
        }
        const quint32 service32 = service.toUInt32(&ok);
        if (ok) {
            services32 << service32;
            continue;
        }

        // QBluetoothUuid::toUInt128() is always Big-Endian
        // convert it to host order
        quint128 hostOrder;
        quint128 qtUuidOrder = service.toUInt128();
        ntoh128(&qtUuidOrder, &hostOrder);
        services128 << hostOrder;
    }
    addServicesData(dest, services16);
    addServicesData(dest, services32);
    addServicesData(dest, services128);
}

void QLeAdvertiserBluez::setManufacturerData(const QLowEnergyAdvertisingData &src, AdvData &dest)
{
    if (src.manufacturerId() == QLowEnergyAdvertisingData::invalidManufacturerId())
        return;
    if (dest.length >= sizeof dest.data - 1 - 1 - 2 - src.manufacturerData().count()) {
        qCWarning(QT_BT_BLUEZ) << "manufacturer data does not fit into advertising data packet";
        return;
    }

    dest.data[dest.length++] = src.manufacturerData().count() + 1 + 2;
    dest.data[dest.length++] = 0xff;
    putBtData(src.manufacturerId(), dest.data + dest.length);
    dest.length += sizeof(quint16);
    std::memcpy(dest.data + dest.length, src.manufacturerData(), src.manufacturerData().count());
    dest.length += src.manufacturerData().count();
}

void QLeAdvertiserBluez::setLocalNameData(const QLowEnergyAdvertisingData &src, AdvData &dest)
{
    if (src.localName().isEmpty())
        return;
    if (dest.length >= sizeof dest.data - 3) {
        qCWarning(QT_BT_BLUEZ) << "local name does not fit into advertising data";
        return;
    }

    const QByteArray localNameUtf8 = src.localName().toUtf8();
    const int fullSize = localNameUtf8.count() + 1 + 1;
    const int size = qMin<int>(fullSize, sizeof dest.data - dest.length);
    const bool isComplete = size == fullSize;
    dest.data[dest.length++] = size - 1;
    const int dataType = isComplete ? 0x9 : 0x8;
    dest.data[dest.length++] = dataType;
    std::memcpy(dest.data + dest.length, localNameUtf8, size - 2);
    dest.length += size - 2;
}

void QLeAdvertiserBluez::setData(bool isScanResponseData)
{
    // Spec v4.2, Vol 3, Part C, 11 and Supplement, Part 1
    AdvData theData;
    static_assert(sizeof theData == 32, "unexpected struct size");
    theData.length = 0;

    const QLowEnergyAdvertisingData &sourceData = isScanResponseData
            ? scanResponseData() : advertisingData();

    if (!sourceData.rawData().isEmpty()) {
        theData.length = qMin<int>(sizeof theData.data, sourceData.rawData().count());
        std::memcpy(theData.data, sourceData.rawData().constData(), theData.length);
    } else {
        if (sourceData.includePowerLevel())
            setPowerLevel(theData);
        if (!isScanResponseData)
            setFlags(theData);

        // Insert new constant-length data here.

        setLocalNameData(sourceData, theData);
        setServicesData(sourceData, theData);
        setManufacturerData(sourceData, theData);
    }

    std::memset(theData.data + theData.length, 0, sizeof theData.data - theData.length);
    const QByteArray dataToSend = byteArrayFromStruct(theData);

    if (!isScanResponseData) {
        qCDebug(QT_BT_BLUEZ) << "advertising data:" << dataToSend.toHex();
        queueCommand(OcfLeSetAdvData, dataToSend);
    } else if ((parameters().mode() == QLowEnergyAdvertisingParameters::AdvScanInd
               || parameters().mode() == QLowEnergyAdvertisingParameters::AdvInd)
               && theData.length > 0) {
        qCDebug(QT_BT_BLUEZ) << "scan response data:" << dataToSend.toHex();
        queueCommand(OcfLeSetScanResponseData, dataToSend);
    }
}

void QLeAdvertiserBluez::setAdvertisingData()
{
    // Spec v4.2, Vol 2, Part E, 7.8.7
    setData(false);
}

void QLeAdvertiserBluez::setScanResponseData()
{
    // Spec v4.2, Vol 2, Part E, 7.8.8
    setData(true);
}

void QLeAdvertiserBluez::setWhiteList()
{
    // Spec v4.2, Vol 2, Part E, 7.8.15-16
    if (parameters().filterPolicy() == QLowEnergyAdvertisingParameters::IgnoreWhiteList)
        return;
    queueCommand(OcfLeClearWhiteList, QByteArray());
    foreach (const auto &addressInfo, parameters().whiteList()) {
        WhiteListParams commandParam;
        static_assert(sizeof commandParam == 7, "unexpected struct size");
        commandParam.addrType = addressInfo.type;
        convertAddress(addressInfo.address.toUInt64(), commandParam.addr.b);
        queueCommand(OcfLeAddToWhiteList, byteArrayFromStruct(commandParam));
    }
}

void QLeAdvertiserBluez::handleCommandCompleted(quint16 opCode, quint8 status,
                                                const QByteArray &data)
{
    if (m_pendingCommands.isEmpty())
        return;
    const quint16 ocf = ocfFromOpCode(opCode);
    if (m_pendingCommands.first().ocf != ocf)
        return; // Not one of our commands.
    m_pendingCommands.takeFirst();
    if (status != 0) {
        qCDebug(QT_BT_BLUEZ) << "command" << ocf << "failed with status" << status;
        if (ocf == OcfLeSetAdvEnable && !m_disableCommandFinished && status == 0xc) {
            qCDebug(QT_BT_BLUEZ) << "initial advertising disable failed, ignoring";
            m_disableCommandFinished = true;
            sendNextCommand();
            return;
        }
        if (ocf == OcfLeReadTxPowerLevel) {
            qCDebug(QT_BT_BLUEZ) << "reading power level failed, leaving it out of the "
                                    "advertising data";
            m_sendPowerLevel = false;
        } else {
            handleError();
            return;
        }
    } else {
        qCDebug(QT_BT_BLUEZ) << "command" << ocf << "executed successfully";
    }

    switch (ocf) {
    case OcfLeReadTxPowerLevel:
        if (m_sendPowerLevel) {
            m_powerLevel = data.at(0);
            qCDebug(QT_BT_BLUEZ) << "TX power level is" << m_powerLevel;
        }
        queueAdvertisingCommands();
        break;
    case OcfLeSetAdvEnable:
        if (!m_disableCommandFinished)
            m_disableCommandFinished = true;
        break;
    default:
        break;
    }

    sendNextCommand();
}

void QLeAdvertiserBluez::handleError()
{
    m_pendingCommands.clear();
    // TODO: Unmonitor event
    emit errorOccurred();
}

QT_END_NAMESPACE
