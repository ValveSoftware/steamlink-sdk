/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include "simulatorgesturescommon_p.h"

#include <QtSimulator/version.h>
#include <QtSimulator/QtSimulator>

#include <QDebug>
#include <QStringList>

using namespace Simulator;


Q_GLOBAL_STATIC(QString, qtSensorGestureData)

SensorGesturesConnection::SensorGesturesConnection(QObject *parent)
    : QObject(parent)
{
    mConnection = new Connection(Connection::Client, QLatin1String("QtSimulator_Mobility_ServerName1.3.0.0"),
                                 0xbeef+1, Version(1,0,0,0), this);
    mWorker = mConnection->connectToServer(Connection::simulatorHostName(true), 0xbeef+1);

    if (!mWorker) {
        qWarning() << "Could not connect to server";
        return;
    }

    mWorker->addReceiver(this);
    mWorker->call("setRequestsSensorGestures");
}

SensorGesturesConnection::~SensorGesturesConnection()
{
    mWorker->call("setSensorGestures", QStringList());
    delete mWorker;
}

void SensorGesturesConnection::setSensorGestureData(const QString &data)
{
    QString gesture = data;
    if (data.contains(QLatin1String("detected"))) {
            gesture.remove(QLatin1String("detected("));
            gesture.remove(QLatin1String(")"));
    }
    *qtSensorGestureData() = gesture;
}

void SensorGesturesConnection::newSensorGestureDetected()
{
    emit sensorGestureDetected();
}

void SensorGesturesConnection::newSensorGestures(const QStringList &gestures)
{
    if (!mWorker) return;

    Q_FOREACH (const QString &gest, gestures) {
        if (!gest.contains(QLatin1String("detected"))) {
            QString tmp = gest.left(gest.length()-2);
            if (!allGestures.contains(tmp)) {
                allGestures.append(tmp);
            }
        }
    }

    mWorker->call("setSensorGestures", allGestures);
}

void SensorGesturesConnection::removeSensorGestures(const QStringList &gestures)
{
    Q_FOREACH (const QString &gest, gestures) {
        QString tmp = gest.left(gest.length()-2);
        if (allGestures.contains(tmp)) {
            allGestures.removeOne(tmp);
        }
    }
    mWorker->call("setSensorGestures", allGestures);
}

QString get_qtSensorGestureData()
{
    return *qtSensorGestureData();
}

