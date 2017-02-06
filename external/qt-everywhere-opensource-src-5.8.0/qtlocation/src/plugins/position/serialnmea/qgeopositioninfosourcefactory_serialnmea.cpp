/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#include "qgeopositioninfosourcefactory_serialnmea.h"
#include <QtPositioning/qnmeapositioninfosource.h>
#include <QtSerialPort/qserialport.h>
#include <QtSerialPort/qserialportinfo.h>
#include <QtCore/qloggingcategory.h>
#include <QSet>

Q_LOGGING_CATEGORY(lcSerial, "qt.positioning.serialnmea")

class NmeaSource : public QNmeaPositionInfoSource
{
public:
    NmeaSource(QObject *parent);
    bool isValid() const { return !m_port.isNull(); }

private:
    QScopedPointer<QSerialPort> m_port;
};

NmeaSource::NmeaSource(QObject *parent)
    : QNmeaPositionInfoSource(RealTimeMode, parent),
      m_port(new QSerialPort)
{
    QByteArray requestedPort = qgetenv("QT_NMEA_SERIAL_PORT");
    if (requestedPort.isEmpty()) {
        const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
        qCDebug(lcSerial) << "Found" << ports.count() << "serial ports";
        if (ports.isEmpty()) {
            qWarning("serialnmea: No serial ports found");
            m_port.reset();
            return;
        }

        // Try to find a well-known device.
        QSet<int> supportedDevices;
        supportedDevices << 0x67b; // GlobalSat (BU-353S4 and probably others)
        supportedDevices << 0xe8d; // Qstarz MTK II
        QString portName;
        foreach (const QSerialPortInfo& port, ports) {
            if (port.hasVendorIdentifier() && supportedDevices.contains(port.vendorIdentifier())) {
                portName = port.portName();
                break;
            }
        }

        if (portName.isEmpty()) {
            qWarning("serialnmea: No known GPS device found. Specify the COM port via QT_NMEA_SERIAL_PORT.");
            m_port.reset();
            return;
        }

        m_port->setPortName(portName);
    } else {
        m_port->setPortName(QString::fromUtf8(requestedPort));
    }

    m_port->setBaudRate(4800);

    qCDebug(lcSerial) << "Opening serial port" << m_port->portName();

    if (!m_port->open(QIODevice::ReadOnly)) {
        qWarning("serialnmea: Failed to open %s", qPrintable(m_port->portName()));
        m_port.reset();
        return;
    }

    setDevice(m_port.data());

    qCDebug(lcSerial) << "Opened successfully";
}

QGeoPositionInfoSource *QGeoPositionInfoSourceFactorySerialNmea::positionInfoSource(QObject *parent)
{
    QScopedPointer<NmeaSource> src(new NmeaSource(parent));
    return src->isValid() ? src.take() : Q_NULLPTR;
}

QGeoSatelliteInfoSource *QGeoPositionInfoSourceFactorySerialNmea::satelliteInfoSource(QObject *parent)
{
    Q_UNUSED(parent);
    return Q_NULLPTR;
}

QGeoAreaMonitorSource *QGeoPositionInfoSourceFactorySerialNmea::areaMonitor(QObject *parent)
{
    Q_UNUSED(parent);
    return Q_NULLPTR;
}
