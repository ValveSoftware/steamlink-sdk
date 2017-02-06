/****************************************************************************
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

#include "qbluetoothservicediscoveryagent.h"
#include "qbluetoothservicediscoveryagent_p.h"

#include "bluez/manager_p.h"
#include "bluez/adapter_p.h"
#include "bluez/device_p.h"
#include "bluez/bluez5_helper_p.h"
#include "bluez/objectmanager_p.h"
#include "bluez/adapter1_bluez5_p.h"

#include <QtCore/QFile>
#include <QtCore/QLibraryInfo>
#include <QtCore/QLoggingCategory>
#include <QtCore/QProcess>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtConcurrent/QtConcurrentRun>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_BLUEZ)

static inline void convertAddress(quint64 from, quint8 (&to)[6])
{
    to[0] = (from >> 0) & 0xff;
    to[1] = (from >> 8) & 0xff;
    to[2] = (from >> 16) & 0xff;
    to[3] = (from >> 24) & 0xff;
    to[4] = (from >> 32) & 0xff;
    to[5] = (from >> 40) & 0xff;
}

QBluetoothServiceDiscoveryAgentPrivate::QBluetoothServiceDiscoveryAgentPrivate(
    QBluetoothServiceDiscoveryAgent *qp, const QBluetoothAddress &deviceAdapter)
:   error(QBluetoothServiceDiscoveryAgent::NoError), m_deviceAdapterAddress(deviceAdapter), state(Inactive), deviceDiscoveryAgent(0),
    mode(QBluetoothServiceDiscoveryAgent::MinimalDiscovery), singleDevice(false),
    manager(0), managerBluez5(0), adapter(0), device(0), sdpScannerProcess(0),
    q_ptr(qp)
{
    if (isBluez5()) {
        managerBluez5 = new OrgFreedesktopDBusObjectManagerInterface(
                                    QStringLiteral("org.bluez"), QStringLiteral("/"),
                                    QDBusConnection::systemBus());
        qRegisterMetaType<QBluetoothServiceDiscoveryAgent::Error>();
    } else {
        qRegisterMetaType<ServiceMap>();
        qDBusRegisterMetaType<ServiceMap>();

        manager = new OrgBluezManagerInterface(QStringLiteral("org.bluez"), QStringLiteral("/"),
                                               QDBusConnection::systemBus());
    }
}

QBluetoothServiceDiscoveryAgentPrivate::~QBluetoothServiceDiscoveryAgentPrivate()
{
    delete device;
    delete manager;
    delete managerBluez5;
    delete adapter;
}

void QBluetoothServiceDiscoveryAgentPrivate::start(const QBluetoothAddress &address)
{
    Q_Q(QBluetoothServiceDiscoveryAgent);

    qCDebug(QT_BT_BLUEZ) << "Discovery on: " << address.toString() << "Mode:" << DiscoveryMode();

    if (managerBluez5) {
        startBluez5(address);
        return;
    }

    QDBusPendingReply<QDBusObjectPath> reply;
    if (m_deviceAdapterAddress.isNull())
        reply = manager->DefaultAdapter();
    else
        reply = manager->FindAdapter(m_deviceAdapterAddress.toString());

    reply.waitForFinished();
    if (reply.isError()) {
        error = QBluetoothServiceDiscoveryAgent::InputOutputError;
        errorString = QBluetoothServiceDiscoveryAgent::tr("Unable to find appointed local adapter");
        emit q->error(error);
        _q_serviceDiscoveryFinished();
        return;
    }

    adapter = new OrgBluezAdapterInterface(QStringLiteral("org.bluez"), reply.value().path(),
                                           QDBusConnection::systemBus());

    if (m_deviceAdapterAddress.isNull()) {
        QDBusPendingReply<QVariantMap> reply = adapter->GetProperties();
        reply.waitForFinished();
        if (!reply.isError()) {
            const QBluetoothAddress path_address(reply.value().value(QStringLiteral("Address")).toString());
            m_deviceAdapterAddress = path_address;
        }
    }

    QDBusPendingReply<QDBusObjectPath> deviceObjectPath = adapter->FindDevice(address.toString());

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(deviceObjectPath, q);
    watcher->setProperty("_q_BTaddress", QVariant::fromValue(address));
    QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                     q, SLOT(_q_foundDevice(QDBusPendingCallWatcher*)));
}

// Bluez 5
void QBluetoothServiceDiscoveryAgentPrivate::startBluez5(const QBluetoothAddress &address)
{
    Q_Q(QBluetoothServiceDiscoveryAgent);

    if (foundHostAdapterPath.isEmpty()) {
        // check that we match adapter addresses or use first if it wasn't specified

        bool ok = false;
        foundHostAdapterPath  = findAdapterForAddress(m_deviceAdapterAddress, &ok);
        if (!ok) {
            discoveredDevices.clear();
            error = QBluetoothServiceDiscoveryAgent::InputOutputError;
            errorString = QBluetoothDeviceDiscoveryAgent::tr("Cannot access adapter during service discovery");
            emit q->error(error);
            _q_serviceDiscoveryFinished();
            return;
        }

        if (foundHostAdapterPath.isEmpty()) {
            // Cannot find a local adapter
            // Abort any outstanding discoveries
            discoveredDevices.clear();

            error = QBluetoothServiceDiscoveryAgent::InvalidBluetoothAdapterError;
            errorString = QBluetoothServiceDiscoveryAgent::tr("Cannot find local Bluetooth adapter");
            emit q->error(error);
            _q_serviceDiscoveryFinished();

            return;
        }
    }

    // ensure we didn't go offline yet
    OrgBluezAdapter1Interface adapter(QStringLiteral("org.bluez"),
                                      foundHostAdapterPath, QDBusConnection::systemBus());
    if (!adapter.powered()) {
        discoveredDevices.clear();

        error = QBluetoothServiceDiscoveryAgent::PoweredOffError;
        errorString = QBluetoothServiceDiscoveryAgent::tr("Local device is powered off");
        emit q->error(error);

        _q_serviceDiscoveryFinished();
        return;
    }

    if (DiscoveryMode() == QBluetoothServiceDiscoveryAgent::MinimalDiscovery) {
        performMinimalServiceDiscovery(address);
    } else {
        runExternalSdpScan(address, QBluetoothAddress(adapter.address()));
    }
}

/* Bluez 5
 * src/tools/sdpscanner performs an SDP scan. This is
 * done out-of-process to avoid license issues. At this stage Bluez uses GPLv2.
 */
void QBluetoothServiceDiscoveryAgentPrivate::runExternalSdpScan(
        const QBluetoothAddress &remoteAddress, const QBluetoothAddress &localAddress)
{
    Q_Q(QBluetoothServiceDiscoveryAgent);

    if (!sdpScannerProcess) {
        const QString binPath = QLibraryInfo::location(QLibraryInfo::BinariesPath);
        QFileInfo fileInfo(binPath, QStringLiteral("sdpscanner"));
        if (!fileInfo.exists() || !fileInfo.isExecutable()) {
            _q_finishSdpScan(QBluetoothServiceDiscoveryAgent::InputOutputError,
                             QBluetoothServiceDiscoveryAgent::tr("Unable to find sdpscanner"),
                             QStringList());
            qCWarning(QT_BT_BLUEZ) << "Cannot find sdpscanner:"
                                   << fileInfo.canonicalFilePath();
            return;
        }

        sdpScannerProcess = new QProcess(q);
        sdpScannerProcess->setReadChannel(QProcess::StandardOutput);
        if (QT_BT_BLUEZ().isDebugEnabled())
            sdpScannerProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
        sdpScannerProcess->setProgram(fileInfo.canonicalFilePath());
        q->connect(sdpScannerProcess, SIGNAL(finished(int,QProcess::ExitStatus)),
                   q, SLOT(_q_sdpScannerDone(int,QProcess::ExitStatus)));
    }

    QStringList arguments;
    arguments << remoteAddress.toString() << localAddress.toString();

    // No filter implies PUBLIC_BROWSE_GROUP based SDP scan
    if (!uuidFilter.isEmpty()) {
        arguments << QLatin1String("-u"); // cmd line option for list of uuids
        foreach (const QBluetoothUuid& uuid, uuidFilter)
            arguments << uuid.toString();
    }

    sdpScannerProcess->setArguments(arguments);
    sdpScannerProcess->start();
}

// Bluez 5
void QBluetoothServiceDiscoveryAgentPrivate::_q_sdpScannerDone(int exitCode, QProcess::ExitStatus status)
{
    if (status != QProcess::NormalExit || exitCode != 0) {
        qCWarning(QT_BT_BLUEZ) << "SDP scan failure" << status << exitCode;
        if (singleDevice) {
            _q_finishSdpScan(QBluetoothServiceDiscoveryAgent::InputOutputError,
                             QBluetoothServiceDiscoveryAgent::tr("Unable to perform SDP scan"),
                             QStringList());
        } else {
            // go to next device
            _q_finishSdpScan(QBluetoothServiceDiscoveryAgent::NoError, QString(), QStringList());
        }
        return;
    }

    QStringList xmlRecords;
    const QByteArray output = sdpScannerProcess->readAllStandardOutput();
    const QString decodedData = QString::fromUtf8(QByteArray::fromBase64(output));

    // split the various xml docs up
    int next;
    int start = decodedData.indexOf(QStringLiteral("<?xml"), 0);
    if (start != -1) {
        do {
            next = decodedData.indexOf(QStringLiteral("<?xml"), start + 1);
            if (next != -1)
                xmlRecords.append(decodedData.mid(start, next-start));
            else
                xmlRecords.append(decodedData.mid(start, decodedData.size() - start));
            start = next;
        } while ( start != -1);
    }

    _q_finishSdpScan(QBluetoothServiceDiscoveryAgent::NoError, QString(), xmlRecords);
}

// Bluez 5
void QBluetoothServiceDiscoveryAgentPrivate::_q_finishSdpScan(QBluetoothServiceDiscoveryAgent::Error errorCode,
                                                              const QString &errorDescription,
                                                              const QStringList &xmlRecords)
{
    Q_Q(QBluetoothServiceDiscoveryAgent);

    if (errorCode != QBluetoothServiceDiscoveryAgent::NoError) {
        qCWarning(QT_BT_BLUEZ) << "SDP search failed for"
                              << (!discoveredDevices.isEmpty()
                                     ? discoveredDevices.at(0).address().toString()
                                     : QStringLiteral("<Unknown>"));
        // We have an error which we need to indicate and stop further processing
        discoveredDevices.clear();
        error = errorCode;
        errorString = errorDescription;
        emit q->error(error);
    } else if (!xmlRecords.isEmpty() && discoveryState() != Inactive) {
        foreach (const QString &record, xmlRecords) {
            const QBluetoothServiceInfo serviceInfo = parseServiceXml(record);

            //apply uuidFilter
            if (!uuidFilter.isEmpty()) {
                bool serviceNameMatched = uuidFilter.contains(serviceInfo.serviceUuid());
                bool serviceClassMatched = false;
                foreach (const QBluetoothUuid &id, serviceInfo.serviceClassUuids()) {
                    if (uuidFilter.contains(id)) {
                        serviceClassMatched = true;
                        break;
                    }
                }

                if (!serviceNameMatched && !serviceClassMatched)
                    continue;
            }

            if (!serviceInfo.isValid())
                continue;

            if (!isDuplicatedService(serviceInfo)) {
                discoveredServices.append(serviceInfo);
                qCDebug(QT_BT_BLUEZ) << "Discovered services" << discoveredDevices.at(0).address().toString()
                                     << serviceInfo.serviceName() << serviceInfo.serviceUuid()
                                     << ">>>" << serviceInfo.serviceClassUuids();

                emit q->serviceDiscovered(serviceInfo);
            }
        }
    }

    _q_serviceDiscoveryFinished();
}

void QBluetoothServiceDiscoveryAgentPrivate::stop()
{
    qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO << "Stop called";
    if (device) {
        //we are waiting for _q_discoveredServices() slot to be called
        // adapter is already 0
        QDBusPendingReply<> reply = device->CancelDiscovery();
        reply.waitForFinished();

        device->deleteLater();
        device = 0;
        Q_ASSERT(!adapter);
    } else if (adapter) {
        //we are waiting for _q_createdDevice() slot to be called
        adapter->deleteLater();
        adapter = 0;
        Q_ASSERT(!device);
    }


    discoveredDevices.clear();
    setDiscoveryState(Inactive);

    // must happen after discoveredDevices.clear() above to avoid retrigger of next scan
    // while waitForFinished() is waiting
    if (sdpScannerProcess) { // Bluez 5
        if (sdpScannerProcess->state() != QProcess::NotRunning) {
            sdpScannerProcess->kill();
            sdpScannerProcess->waitForFinished();
        }
    }

    Q_Q(QBluetoothServiceDiscoveryAgent);
    emit q->canceled();
}

void QBluetoothServiceDiscoveryAgentPrivate::_q_foundDevice(QDBusPendingCallWatcher *watcher)
{
    if (!adapter) {
        watcher->deleteLater();
        return;
    }

    Q_Q(QBluetoothServiceDiscoveryAgent);

    const QBluetoothAddress &address = watcher->property("_q_BTaddress").value<QBluetoothAddress>();

    qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO << "found" << address.toString();

    QDBusPendingReply<QDBusObjectPath> deviceObjectPath = *watcher;
    watcher->deleteLater();
    if (deviceObjectPath.isError()) {
        if (deviceObjectPath.error().name() != QStringLiteral("org.bluez.Error.DoesNotExist")) {
            qCDebug(QT_BT_BLUEZ) << "Find device failed Error: " << error << deviceObjectPath.error().name();
            delete adapter;
            adapter = 0;
            if (singleDevice) {
                error = QBluetoothServiceDiscoveryAgent::InputOutputError;
                errorString = QBluetoothServiceDiscoveryAgent::tr("Unable to access device");
                emit q->error(error);
            }
            _q_serviceDiscoveryFinished();
            return;
        }

        deviceObjectPath = adapter->CreateDevice(address.toString());
        watcher = new QDBusPendingCallWatcher(deviceObjectPath, q);
        watcher->setProperty("_q_BTaddress",  QVariant::fromValue(address));
        QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                         q, SLOT(_q_createdDevice(QDBusPendingCallWatcher*)));
        return;
    }

    qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO << "path" << deviceObjectPath.value().path();
    discoverServices(deviceObjectPath.value().path());
}

void QBluetoothServiceDiscoveryAgentPrivate::_q_createdDevice(QDBusPendingCallWatcher *watcher)
{
    Q_Q(QBluetoothServiceDiscoveryAgent);

    if (!adapter) {
        watcher->deleteLater();
        return;
    }

    const QBluetoothAddress &address = watcher->property("_q_BTaddress").value<QBluetoothAddress>();

    qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO << "created" << address.toString();

    QDBusPendingReply<QDBusObjectPath> deviceObjectPath = *watcher;
    watcher->deleteLater();
    if (deviceObjectPath.isError()) {
        if (deviceObjectPath.error().name() != QLatin1String("org.bluez.Error.AlreadyExists")) {
            qCDebug(QT_BT_BLUEZ) << "Create device failed Error: " << error << deviceObjectPath.error().name();
            delete adapter;
            adapter = 0;
            if (singleDevice) {
                error = QBluetoothServiceDiscoveryAgent::InputOutputError;
                errorString = QBluetoothServiceDiscoveryAgent::tr("Unable to access device");
                emit q->error(error);
            }
            _q_serviceDiscoveryFinished();
            return;
        }
    }

    qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO << "path" << deviceObjectPath.value().path();
    discoverServices(deviceObjectPath.value().path());
}

void QBluetoothServiceDiscoveryAgentPrivate::discoverServices(const QString &deviceObjectPath)
{
    Q_Q(QBluetoothServiceDiscoveryAgent);

    device = new OrgBluezDeviceInterface(QStringLiteral("org.bluez"),
                                         deviceObjectPath,
                                         QDBusConnection::systemBus());
    delete adapter;
    adapter = 0;

    QVariantMap deviceProperties;
    QString classType;
    QDBusPendingReply<QVariantMap> deviceReply = device->GetProperties();
    deviceReply.waitForFinished();
    if (!deviceReply.isError()) {
        deviceProperties = deviceReply.value();
        classType = deviceProperties.value(QStringLiteral("Class")).toString();
    }

    /*
     * Low Energy services in bluez are represented as the list of the paths that can be
     * accessed with org.bluez.Characteristic
     */
    //QDBusArgument services = v.value(QLatin1String("Services")).value<QDBusArgument>();


    /*
     * Bluez v4.1 does not have extra bit which gives information if device is Bluetooth
     * Low Energy device and the way to discover it is with Class property of the Bluetooth device.
     * Low Energy devices do not have property Class.
     * In case we have LE device finish service discovery; otherwise search for regular services.
     */
    if (classType.isEmpty()) { //is BLE device or device properties above not retrievable
        qCDebug(QT_BT_BLUEZ) << "Discovered BLE-only device. Normal service discovery skipped.";
        delete device;
        device = 0;

        const QStringList deviceUuids = deviceProperties.value(QStringLiteral("UUIDs")).toStringList();
        for (int i = 0; i < deviceUuids.size(); i++) {
            QString b = deviceUuids.at(i);
            b = b.remove(QLatin1Char('{')).remove(QLatin1Char('}'));
            const QBluetoothUuid uuid(b);

            qCDebug(QT_BT_BLUEZ) << "Discovered service" << uuid << uuidFilter.size();
            QBluetoothServiceInfo service;
            service.setDevice(discoveredDevices.at(0));
            bool ok = false;
            quint16 serviceClass = uuid.toUInt16(&ok);
            if (ok)
                service.setServiceName(QBluetoothUuid::serviceClassToString(
                                           static_cast<QBluetoothUuid::ServiceClassUuid>(serviceClass)));

            QBluetoothServiceInfo::Sequence classId;
            classId << QVariant::fromValue(uuid);
            service.setAttribute(QBluetoothServiceInfo::ServiceClassIds, classId);

            QBluetoothServiceInfo::Sequence protocolDescriptorList;
            {
                QBluetoothServiceInfo::Sequence protocol;
                protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::L2cap));
                protocolDescriptorList.append(QVariant::fromValue(protocol));
            }
            {
                QBluetoothServiceInfo::Sequence protocol;
                protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::Att));
                protocolDescriptorList.append(QVariant::fromValue(protocol));
            }
            service.setAttribute(QBluetoothServiceInfo::ProtocolDescriptorList, protocolDescriptorList);

            if (uuidFilter.isEmpty())
                emit q->serviceDiscovered(service);
            else {
                for (int j = 0; j < uuidFilter.size(); j++) {
                    if (uuidFilter.at(j) == uuid)
                        emit q->serviceDiscovered(service);
                }
            }
        }

        if (singleDevice && deviceReply.isError()) {
            error = QBluetoothServiceDiscoveryAgent::InputOutputError;
            errorString = QBluetoothServiceDiscoveryAgent::tr("Unable to access device");
            emit q->error(error);
        }
        _q_serviceDiscoveryFinished();
    } else {
        QString pattern;
        foreach (const QBluetoothUuid &uuid, uuidFilter)
            pattern += uuid.toString().remove(QLatin1Char('{')).remove(QLatin1Char('}')) + QLatin1Char(' ');

        pattern = pattern.trimmed();
        qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO << "Discover restrictions:" << pattern;

        QDBusPendingReply<ServiceMap> discoverReply = device->DiscoverServices(pattern);
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(discoverReply, q);
        QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                         q, SLOT(_q_discoveredServices(QDBusPendingCallWatcher*)));
    }
}

// Bluez 4
void QBluetoothServiceDiscoveryAgentPrivate::_q_discoveredServices(QDBusPendingCallWatcher *watcher)
{
    if (!device) {
        watcher->deleteLater();
        return;
    }

    qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO;
    Q_Q(QBluetoothServiceDiscoveryAgent);

    QDBusPendingReply<ServiceMap> reply = *watcher;
    if (reply.isError()) {
        qCDebug(QT_BT_BLUEZ) << "discoveredServices error: " << error << reply.error().message();
        watcher->deleteLater();
        if (singleDevice) {
            error = QBluetoothServiceDiscoveryAgent::UnknownError;
            errorString = reply.error().message();
            emit q->error(error);
        }
        delete device;
        device = 0;
        _q_serviceDiscoveryFinished();
        return;
    }

    ServiceMap map = reply.value();

    qCDebug(QT_BT_BLUEZ) << "Parsing xml" << discoveredDevices.at(0).address().toString() << discoveredDevices.count() << map.count();



    foreach (const QString &record, reply.value()) {
        QBluetoothServiceInfo serviceInfo = parseServiceXml(record);

        if (!serviceInfo.isValid())
            continue;

        // Don't need to apply uuidFilter because Bluez 4 applies
        // search pattern during DiscoverServices() call

        Q_Q(QBluetoothServiceDiscoveryAgent);
        // Some service uuids are unknown to Bluez. In such cases we fall back
        // to our own naming resolution.
        if (serviceInfo.serviceName().isEmpty()
            && !serviceInfo.serviceClassUuids().isEmpty()) {
            foreach (const QBluetoothUuid &classUuid, serviceInfo.serviceClassUuids()) {
                bool ok = false;
                QBluetoothUuid::ServiceClassUuid clsId
                    = static_cast<QBluetoothUuid::ServiceClassUuid>(classUuid.toUInt16(&ok));
                if (ok) {
                    serviceInfo.setServiceName(QBluetoothUuid::serviceClassToString(clsId));
                    break;
                }
            }
        }

        if (!isDuplicatedService(serviceInfo)) {
            discoveredServices.append(serviceInfo);
            qCDebug(QT_BT_BLUEZ) << "Discovered services" << discoveredDevices.at(0).address().toString()
                                 << serviceInfo.serviceName();
            emit q->serviceDiscovered(serviceInfo);
        }

        // could stop discovery, check for state
        if (discoveryState() == Inactive)
            qCDebug(QT_BT_BLUEZ) << "Exit discovery after stop";
    }

    watcher->deleteLater();
    delete device;
    device = 0;

    _q_serviceDiscoveryFinished();
}

QBluetoothServiceInfo QBluetoothServiceDiscoveryAgentPrivate::parseServiceXml(
                            const QString& xmlRecord)
{
    QXmlStreamReader xml(xmlRecord);

    QBluetoothServiceInfo serviceInfo;
    serviceInfo.setDevice(discoveredDevices.at(0));

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.tokenType() == QXmlStreamReader::StartElement &&
            xml.name() == QLatin1String("attribute")) {
            quint16 attributeId =
                xml.attributes().value(QLatin1String("id")).toString().toUShort(0, 0);

            if (xml.readNextStartElement()) {
                const QVariant value = readAttributeValue(xml);
                serviceInfo.setAttribute(attributeId, value);
            }
        }
    }

    return serviceInfo;
}

// Bluez 5
void QBluetoothServiceDiscoveryAgentPrivate::performMinimalServiceDiscovery(const QBluetoothAddress &deviceAddress)
{
    if (foundHostAdapterPath.isEmpty()) {
        _q_serviceDiscoveryFinished();
        return;
    }

    Q_Q(QBluetoothServiceDiscoveryAgent);

    QDBusPendingReply<ManagedObjectList> reply = managerBluez5->GetManagedObjects();
    reply.waitForFinished();
    if (reply.isError()) {
        if (singleDevice) {
            error = QBluetoothServiceDiscoveryAgent::InputOutputError;
            errorString = reply.error().message();
            emit q->error(error);

        }
        _q_serviceDiscoveryFinished();
        return;
    }

    QStringList uuidStrings;

    ManagedObjectList managedObjectList = reply.value();
    for (ManagedObjectList::const_iterator it = managedObjectList.constBegin(); it != managedObjectList.constEnd(); ++it) {
        const InterfaceList &ifaceList = it.value();

        for (InterfaceList::const_iterator jt = ifaceList.constBegin(); jt != ifaceList.constEnd(); ++jt) {
            const QString &iface = jt.key();
            const QVariantMap &ifaceValues = jt.value();

            if (iface == QStringLiteral("org.bluez.Device1")) {
                if (deviceAddress.toString() == ifaceValues.value(QStringLiteral("Address")).toString()) {
                    uuidStrings = ifaceValues.value(QStringLiteral("UUIDs")).toStringList();
                    break;
                }
            }
        }
        if (!uuidStrings.isEmpty())
            break;
    }

    if (uuidStrings.isEmpty() || discoveredDevices.isEmpty()) {
        qCWarning(QT_BT_BLUEZ) << "No uuids found for" << deviceAddress.toString();
         // nothing found -> go to next uuid
        _q_serviceDiscoveryFinished();
        return;
    }

    qCDebug(QT_BT_BLUEZ) << "Minimal uuid list for" << deviceAddress.toString() << uuidStrings;

    QBluetoothUuid uuid;
    for (int i = 0; i < uuidStrings.count(); i++) {
        uuid = QBluetoothUuid(uuidStrings.at(i));
        if (uuid.isNull())
            continue;

        //apply uuidFilter
        if (!uuidFilter.isEmpty() && !uuidFilter.contains(uuid))
            continue;

        QBluetoothServiceInfo serviceInfo;
        serviceInfo.setDevice(discoveredDevices.at(0));

        if (uuid.minimumSize() == 16) { // not derived from Bluetooth Base UUID
            serviceInfo.setServiceUuid(uuid);
            serviceInfo.setServiceName(QBluetoothServiceDiscoveryAgent::tr("Custom Service"));
        } else {
            // set uuid as service class id
            QBluetoothServiceInfo::Sequence classId;
            classId << QVariant::fromValue(uuid);
            serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceClassIds, classId);
            QBluetoothUuid::ServiceClassUuid clsId
                    = static_cast<QBluetoothUuid::ServiceClassUuid>(uuid.data1 & 0xffff);
            serviceInfo.setServiceName(QBluetoothUuid::serviceClassToString(clsId));
        }

        QBluetoothServiceInfo::Sequence protocolDescriptorList;
        {
            QBluetoothServiceInfo::Sequence protocol;
            protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::L2cap));
            protocolDescriptorList.append(QVariant::fromValue(protocol));
        }
        {
            QBluetoothServiceInfo::Sequence protocol;
            protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::Att));
            protocolDescriptorList.append(QVariant::fromValue(protocol));
        }
        serviceInfo.setAttribute(QBluetoothServiceInfo::ProtocolDescriptorList, protocolDescriptorList);

        //don't include the service if we already discovered it before
        if (!isDuplicatedService(serviceInfo)) {
            discoveredServices << serviceInfo;
            qCDebug(QT_BT_BLUEZ) << "Discovered services" << discoveredDevices.at(0).address().toString()
                                 << serviceInfo.serviceName();
            emit q->serviceDiscovered(serviceInfo);
        }
    }

    _q_serviceDiscoveryFinished();
}

QVariant QBluetoothServiceDiscoveryAgentPrivate::readAttributeValue(QXmlStreamReader &xml)
{
    if (xml.name() == QLatin1String("boolean")) {
        const QString value = xml.attributes().value(QStringLiteral("value")).toString();
        xml.skipCurrentElement();
        return value == QLatin1String("true");
    } else if (xml.name() == QLatin1String("uint8")) {
        quint8 value = xml.attributes().value(QStringLiteral("value")).toString().toUShort(0, 0);
        xml.skipCurrentElement();
        return value;
    } else if (xml.name() == QLatin1String("uint16")) {
        quint16 value = xml.attributes().value(QStringLiteral("value")).toString().toUShort(0, 0);
        xml.skipCurrentElement();
        return value;
    } else if (xml.name() == QLatin1String("uint32")) {
        quint32 value = xml.attributes().value(QStringLiteral("value")).toString().toUInt(0, 0);
        xml.skipCurrentElement();
        return value;
    } else if (xml.name() == QLatin1String("uint64")) {
        quint64 value = xml.attributes().value(QStringLiteral("value")).toString().toULongLong(0, 0);
        xml.skipCurrentElement();
        return value;
    } else if (xml.name() == QLatin1String("uuid")) {
        QBluetoothUuid uuid;
        const QString value = xml.attributes().value(QStringLiteral("value")).toString();
        if (value.startsWith(QStringLiteral("0x"))) {
            if (value.length() == 6) {
                quint16 v = value.toUShort(0, 0);
                uuid = QBluetoothUuid(v);
            } else if (value.length() == 10) {
                quint32 v = value.toUInt(0, 0);
                uuid = QBluetoothUuid(v);
            }
        } else {
            uuid = QBluetoothUuid(value);
        }
        xml.skipCurrentElement();
        return QVariant::fromValue(uuid);
    } else if (xml.name() == QLatin1String("text") || xml.name() == QLatin1String("url")) {
        QString value = xml.attributes().value(QStringLiteral("value")).toString();
        if (xml.attributes().value(QStringLiteral("encoding")) == QLatin1String("hex"))
            value = QString::fromUtf8(QByteArray::fromHex(value.toLatin1()));
        xml.skipCurrentElement();
        return value;
    } else if (xml.name() == QLatin1String("sequence")) {
        QBluetoothServiceInfo::Sequence sequence;

        while (xml.readNextStartElement()) {
            QVariant value = readAttributeValue(xml);
            sequence.append(value);
        }

        return QVariant::fromValue<QBluetoothServiceInfo::Sequence>(sequence);
    } else {
        qCWarning(QT_BT_BLUEZ) << "unknown attribute type"
                               << xml.name().toString()
                               << xml.attributes().value(QStringLiteral("value")).toString();
        xml.skipCurrentElement();
        return QVariant();
    }
}

QT_END_NAMESPACE
