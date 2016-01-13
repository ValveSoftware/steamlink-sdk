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

#include <QtSensors/QSensor>

#include "qturnoversensorgesturerecognizer.h"
#include "qtsensorgesturesensorhandler.h"

QT_BEGIN_NAMESPACE

// turnover and put down i.e. facedown

QTurnoverSensorGestureRecognizer::QTurnoverSensorGestureRecognizer(QObject *parent) :
    QSensorGestureRecognizer(parent),
    isClose(0)
  , isFaceDown(0), active(0)
{
}

QTurnoverSensorGestureRecognizer::~QTurnoverSensorGestureRecognizer()
{
}

void QTurnoverSensorGestureRecognizer::create()
{
}

bool QTurnoverSensorGestureRecognizer::start()
{
    if (QtSensorGestureSensorHandler::instance()->startSensor(QtSensorGestureSensorHandler::Proximity)) {
        if (QtSensorGestureSensorHandler::instance()->startSensor(QtSensorGestureSensorHandler::Orientation)) {
            active = true;
            connect(QtSensorGestureSensorHandler::instance(),SIGNAL(proximityReadingChanged(QProximityReading*)),
                    this,SLOT(proximityChanged(QProximityReading*)));

            connect(QtSensorGestureSensorHandler::instance(),SIGNAL(orientationReadingChanged(QOrientationReading*)),
                    this,SLOT(orientationReadingChanged(QOrientationReading*)));
        } else {
            QtSensorGestureSensorHandler::instance()->stopSensor(QtSensorGestureSensorHandler::Proximity);
            active = false;
        }
    } else {
        active = false;
    }
    return active;
}

bool QTurnoverSensorGestureRecognizer::stop()
{
    QtSensorGestureSensorHandler::instance()->stopSensor(QtSensorGestureSensorHandler::Proximity);
    QtSensorGestureSensorHandler::instance()->stopSensor(QtSensorGestureSensorHandler::Orientation);

    disconnect(QtSensorGestureSensorHandler::instance(),SIGNAL(proximityReadingChanged(QProximityReading*)),
            this,SLOT(proximityChanged(QProximityReading*)));
    disconnect(QtSensorGestureSensorHandler::instance(),SIGNAL(orientationReadingChanged(QOrientationReading*)),
            this,SLOT(orientationReadingChanged(QOrientationReading*)));

    active = false;

    return active;
}

bool QTurnoverSensorGestureRecognizer::isActive()
{
    return active;
}

QString QTurnoverSensorGestureRecognizer::id() const
{
    return QString("QtSensors.turnover");
}

void QTurnoverSensorGestureRecognizer::proximityChanged(QProximityReading *reading)
{
    isClose = reading->close();
    if (isClose)
        isRecognized();
}

void QTurnoverSensorGestureRecognizer::orientationReadingChanged(QOrientationReading *reading)
{
    switch (reading->orientation()) {
       case  QOrientationReading::FaceDown:
    {
        isFaceDown = true;
        isRecognized();
    }
        break;
    default:
        isFaceDown = false;
        break;
    };
}

void QTurnoverSensorGestureRecognizer::isRecognized()
{
    if (isClose && isFaceDown) {
        Q_EMIT turnover();
        Q_EMIT detected("turnover");
    }
}
QT_END_NAMESPACE

