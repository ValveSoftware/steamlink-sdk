/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
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
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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

