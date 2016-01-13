/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd
** Contact: lorn.potter@jollamobile.com
**
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "networkcounter.h"
#include <counter.h>
#include <networkservice.h>
#include <QtCore>

NetworkCounter::NetworkCounter(QObject *parent) :
    QObject(parent)
{
    counter = new Counter(this);

    connect(counter,SIGNAL(counterChanged(QString,QVariantMap,bool)),this,SLOT(counterChanged(QString,QVariantMap,bool)));
    connect(counter,SIGNAL(bytesReceivedChanged(quint64)),this,SLOT(bytesReceivedChanged(quint64)));
    connect(counter,SIGNAL(bytesTransmittedChanged(quint64)),this,SLOT(bytesTransmittedChanged(quint64)));
    connect(counter,SIGNAL(secondsOnlineChanged(quint64)),this,SLOT(secondsOnlineChanged(quint64)));
    connect(counter,SIGNAL(roamingChanged(bool)),SLOT(roamingChanged(bool)));

    counter->setRunning(true);
}

void NetworkCounter::counterChanged(const QString servicePath, const QVariantMap &counters,  bool roaming)
{
    QTextStream out(stdout);
    NetworkService service;
    service.setPath(servicePath);

    out << service.name()
        << (roaming ? "  roaming" : "  home")
        << "   bytes received: " << counters["RX.Bytes"].toUInt()
        << "   bytes transmitted: " << counters["TX.Bytes"].toUInt()
        << "   seconds: " << counters["Time"].toUInt()
        << endl;
}

void NetworkCounter::bytesReceivedChanged(quint64 bytesRx)
{
    QTextStream out(stdout);
    out  << Q_FUNC_INFO << " " << bytesRx << endl;
}

void NetworkCounter::bytesTransmittedChanged(quint64 bytesTx)
{
    QTextStream out(stdout);
    out  << Q_FUNC_INFO << " " << bytesTx << endl;
}

void NetworkCounter::secondsOnlineChanged(quint64 seconds)
{
    QTextStream out(stdout);
    out << Q_FUNC_INFO << " " << seconds << endl;
}

void NetworkCounter::roamingChanged(bool roaming)
{
    QTextStream out(stdout);
    out  << Q_FUNC_INFO << " " << roaming << endl;
}
