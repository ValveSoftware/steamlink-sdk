/****************************************************************************
**
** Copyright (C) 2016 Denis Shienkov <denis.shienkov@gmail.com>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
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
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qserialportinfo.h"
#include "qserialportinfo_p.h"
#include "qserialport_p.h"

#include <QtCore/qdatastream.h>
#include <QtCore/qvector.h>
#include <QtCore/qdir.h>

#include <errno.h>
#include <sys/types.h> // kill
#include <signal.h>    // kill

#include <sys/sysctl.h> // sysctl, sysctlnametomib

QT_BEGIN_NAMESPACE

static QString deviceProperty(const QString &source, const QByteArray &pattern)
{
    const int firstbound = source.indexOf(QLatin1String(pattern));
    if (firstbound == -1)
        return QString();
    const int lastbound = source.indexOf(QLatin1Char(' '), firstbound);
    return source.mid(firstbound + pattern.size(), lastbound - firstbound - pattern.size());
}

static QString deviceName(const QString &pnpinfo)
{
    return deviceProperty(pnpinfo, "ttyname=");
}

static QString deviceCount(const QString &pnpinfo)
{
    return deviceProperty(pnpinfo, "ttyports=");
}

static quint16 deviceProductIdentifier(const QString &pnpinfo, bool &hasIdentifier)
{
    QString result = deviceProperty(pnpinfo, "product=");
    return result.toInt(&hasIdentifier, 16);
}

static quint16 deviceVendorIdentifier(const QString &pnpinfo, bool &hasIdentifier)
{
    QString result = deviceProperty(pnpinfo, "vendor=");
    return result.toInt(&hasIdentifier, 16);
}

static QString deviceSerialNumber(const QString &pnpinfo)
{
    QString serialNumber = deviceProperty(pnpinfo, "sernum=");
    serialNumber.remove(QLatin1Char('"'));
    return serialNumber;
}

// A 'desc' string contains the both description and manufacturer
// properties, which are not possible to extract from the source
// string. Besides, this string can contains an other information,
// which should be excluded from the result.
static QString deviceDescriptionAndManufacturer(const QString &desc)
{
    const int classindex = desc.indexOf(QLatin1String(", class "));
    if (classindex == -1)
        return desc;
    return desc.mid(0, classindex);
}

struct NodeInfo
{
    QString name;
    QString value;
};

static QVector<int> mibFromName(const QString &name)
{
    size_t mibsize = 0;
    if (::sysctlnametomib(name.toLocal8Bit().constData(), nullptr, &mibsize) < 0
            || mibsize == 0) {
        return QVector<int>();
    }
    QVector<int> mib(mibsize);
    if (::sysctlnametomib(name.toLocal8Bit().constData(), &mib[0], &mibsize) < 0)
        return QVector<int>();

    return mib;
}

static QVector<int> nextOid(const QVector<int> &previousOid)
{
    QVector<int> mib;
    mib.append(0); // Magic undocumented code (CTL_UNSPEC ?)
    mib.append(2); // Magic undocumented code
    for (int code : previousOid)
        mib.append(code);

    size_t requiredLength = 0;
    if (::sysctl(&mib[0], mib.count(), nullptr, &requiredLength, nullptr, 0) < 0)
        return QVector<int>();
    const size_t oidLength = requiredLength / sizeof(int);
    QVector<int> oid(oidLength, 0);
    if (::sysctl(&mib[0], mib.count(), &oid[0], &requiredLength, nullptr, 0) < 0)
        return QVector<int>();

    if (previousOid.first() != oid.first())
        return QVector<int>();

    return oid;
}

static NodeInfo nodeForOid(const QVector<int> &oid)
{
    QVector<int> mib;
    mib.append(0); // Magic undocumented code (CTL_UNSPEC ?)
    mib.append(1); // Magic undocumented code
    for (int code : oid)
        mib.append(code);

    // query node name
    size_t requiredLength = 0;
    if (::sysctl(&mib[0], mib.count(), nullptr, &requiredLength, nullptr, 0) < 0)
        return NodeInfo();
    QByteArray name(requiredLength, 0);
    if (::sysctl(&mib[0], mib.count(), name.data(), &requiredLength, nullptr, 0) < 0)
        return NodeInfo();

    // query node value
    requiredLength = 0;
    if (::sysctl(&oid[0], oid.count(), nullptr, &requiredLength, nullptr, 0) < 0)
        return NodeInfo();
    QByteArray value(requiredLength, 0);
    if (::sysctl(&oid[0], oid.count(), value.data(), &requiredLength, nullptr, 0) < 0)
        return NodeInfo();

    // query value format
    mib[1] = 4; // Magic undocumented code
    requiredLength = 0;
    if (::sysctl(&mib[0], mib.count(), nullptr, &requiredLength, nullptr, 0) < 0)
        return NodeInfo();
    QByteArray buf(requiredLength, 0);
    if (::sysctl(&mib[0], mib.count(), buf.data(), &requiredLength, nullptr, 0) < 0)
        return NodeInfo();

    QDataStream in(buf);
    in.setByteOrder(QDataStream::LittleEndian);
    quint32 kind = 0;
    qint8 format = 0;
    in >> kind >> format;

    NodeInfo result;

    // we need only the string-type value
    if (format == 'A') {
        result.name = QString::fromLocal8Bit(name.constData());
        result.value = QString::fromLocal8Bit(value.constData());
    }

    return result;
}

static QList<NodeInfo> enumerateDesiredNodes(const QVector<int> &mib)
{
    QList<NodeInfo> nodes;

    QVector<int> oid = mib;

    for (;;) {
        const QVector<int> nextoid = nextOid(oid);
        if (nextoid.isEmpty())
            break;

        const NodeInfo node = nodeForOid(nextoid);
        if (!node.name.isEmpty()) {
            if (node.name.endsWith(QLatin1String("\%desc"))
                    || node.name.endsWith(QLatin1String("\%pnpinfo"))) {
                nodes.append(node);
            }
        }

        oid = nextoid;
    }

    return nodes;
}

QList<QSerialPortInfo> QSerialPortInfo::availablePorts()
{
    const QVector<int> mib = mibFromName(QLatin1String("dev"));
    if (mib.isEmpty())
        return QList<QSerialPortInfo>();

    const QList<NodeInfo> nodes = enumerateDesiredNodes(mib);
    if (nodes.isEmpty())
        return QList<QSerialPortInfo>();

    QDir deviceDir(QLatin1String("/dev"));
    if (!(deviceDir.exists() && deviceDir.isReadable()))
        return QList<QSerialPortInfo>();

    deviceDir.setNameFilters(QStringList() << QLatin1String("cua*") << QLatin1String("tty*"));
    deviceDir.setFilter(QDir::Files | QDir::System | QDir::NoSymLinks);

    QList<QSerialPortInfo> cuaCandidates;
    QList<QSerialPortInfo> ttyCandidates;

    const auto portNames = deviceDir.entryList();
    for (const QString &portName : portNames) {
        if (portName.endsWith(QLatin1String(".init"))
                || portName.endsWith(QLatin1String(".lock"))) {
            continue;
        }

        QSerialPortInfoPrivate priv;
        priv.portName = portName;
        priv.device = QSerialPortInfoPrivate::portNameToSystemLocation(portName);

        for (const NodeInfo &node : nodes) {
            const int pnpinfoindex = node.name.indexOf(QLatin1String("\%pnpinfo"));
            if (pnpinfoindex == -1)
                continue;

            if (node.value.isEmpty())
                continue;

            QString ttyname = deviceName(node.value);
            if (ttyname.isEmpty())
                continue;

            const QString ttyportscount = deviceCount(node.value);
            if (ttyportscount.isEmpty())
                continue;

            const int count = ttyportscount.toInt();
            if (count == 0)
                continue;
            if (count > 1) {
                bool matched = false;
                for (int i = 0; i < count; ++i) {
                    const QString ends = QString(QLatin1String("%1.%2")).arg(ttyname).arg(i);
                    if (portName.endsWith(ends)) {
                        matched = true;
                        break;
                    }
                }

                if (!matched)
                    continue;
            } else {
                if (!portName.endsWith(ttyname))
                    continue;
            }

            priv.serialNumber = deviceSerialNumber(node.value);
            priv.vendorIdentifier = deviceVendorIdentifier(node.value, priv.hasVendorIdentifier);
            priv.productIdentifier = deviceProductIdentifier(node.value, priv.hasProductIdentifier);

            const QString nodebase = node.name.mid(0, pnpinfoindex);
            const QString descnode = QString(QLatin1String("%1\%desc")).arg(nodebase);

            // search for description and manufacturer properties
            for (const NodeInfo &node : nodes) {
                if (node.name != descnode)
                    continue;

                if (node.value.isEmpty())
                    continue;

                // We can not separate the description and the manufacturer
                // properties from the node value, so lets just duplicate it.
                priv.description = deviceDescriptionAndManufacturer(node.value);
                priv.manufacturer = priv.description;
                break;
            }

            break;
        }

        if (portName.startsWith(QLatin1String("cua")))
            cuaCandidates.append(priv);
        else if (portName.startsWith(QLatin1String("tty")))
            ttyCandidates.append(priv);
    }

    QList<QSerialPortInfo> serialPortInfoList;

    for (const QSerialPortInfo &cuaCandidate : qAsConst(cuaCandidates)) {
        const QString cuaPortName = cuaCandidate.portName();
        const QString cuaToken = deviceProperty(cuaPortName, "cua");
        for (const QSerialPortInfo &ttyCandidate : qAsConst(ttyCandidates)) {
            const QString ttyPortName = ttyCandidate.portName();
            const QString ttyToken = deviceProperty(ttyPortName, "tty");
            if (cuaToken != ttyToken)
                continue;

            serialPortInfoList.append(cuaCandidate);
            serialPortInfoList.append(ttyCandidate);
        }
    }

    return serialPortInfoList;
}

bool QSerialPortInfo::isBusy() const
{
    QString lockFilePath = serialPortLockFilePath(portName());
    if (lockFilePath.isEmpty())
        return false;

    QFile reader(lockFilePath);
    if (!reader.open(QIODevice::ReadOnly))
        return false;

    QByteArray pidLine = reader.readLine();
    pidLine.chop(1);
    if (pidLine.isEmpty())
        return false;

    qint64 pid = pidLine.toLongLong();

    if (pid && (::kill(pid, 0) == -1) && (errno == ESRCH))
        return false; // PID doesn't exist anymore

    return true;
}

#if QT_DEPRECATED_SINCE(5, 2)
bool QSerialPortInfo::isValid() const
{
    QFile f(systemLocation());
    return f.exists();
}
#endif // QT_DEPRECATED_SINCE(5, 2)

QString QSerialPortInfoPrivate::portNameToSystemLocation(const QString &source)
{
    return (source.startsWith(QLatin1Char('/'))
            || source.startsWith(QLatin1String("./"))
            || source.startsWith(QLatin1String("../")))
            ? source : (QLatin1String("/dev/") + source);
}

QString QSerialPortInfoPrivate::portNameFromSystemLocation(const QString &source)
{
    return source.startsWith(QLatin1String("/dev/"))
            ? source.mid(5) : source;
}

QT_END_NAMESPACE
