/****************************************************************************
**
** Copyright (C) 2012 Lorn Potter.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtQml/qqml.h>
#include <QtCore/QFile>
#include <QFileInfo>

#include <QTextStream>
#include <QtSensors>
#include <QDir>
#include <QtSensors/QAccelerometer>
#include <QtSensors/QIRProximitySensor>
#include <QtSensors/QOrientationSensor>
#include <QtSensors/QProximitySensor>
#include <QDebug>

#include "collector.h"

Collector::Collector(QObject *parent)
    : QObject(parent),
      accel(0),
      orientation(0),
      proximity(0),
      irProx(0),
      tapSensor(0),
      dataFile(QDir::tempPath()+"/sensordump_0.dat")
    , isActive(0),
      fileCounter(0)
{
    accel = new QAccelerometer(this);
    accel->connectToBackend();
    accel->setDataRate(100);
    connect(accel,SIGNAL(readingChanged()),this,SLOT(accelChanged()));

    orientation = new QOrientationSensor(this);
    orientation->connectToBackend();
    orientation->setDataRate(100);
    connect(orientation,SIGNAL(readingChanged()),this,SLOT(orientationChanged()));

    proximity = new QProximitySensor(this);
    proximity->connectToBackend();
    connect(proximity,SIGNAL(readingChanged()),this,SLOT(proximityChanged()));

    irProx = new QIRProximitySensor(this);
    irProx->connectToBackend();
    irProx->setDataRate(50);
    connect(irProx,SIGNAL(readingChanged()),this,SLOT(irProximityChanged()));

    tapSensor = new QTapSensor(this);
    tapSensor->connectToBackend();
    connect(tapSensor,SIGNAL(readingChanged()),this,SLOT(tapChanged()));
}

Collector::~Collector()
{
}

void Collector::accelChanged()
{
    const qreal x = accel->reading()->x();
    const qreal y = accel->reading()->y();
    const qreal z = accel->reading()->z();
    const quint64 ts = accel->reading()->timestamp();

    QTextStream out(&dataFile);
    out << QString("accelerometer: %1,%2,%3,%4").arg(ts).arg(x).arg(y).arg(z) << "\n";
}

void Collector::orientationChanged()
{
    const QOrientationReading *orientationReading = orientation->reading();
    QOrientationReading::Orientation o = orientationReading->orientation();
    const quint64 ts = orientationReading->timestamp();

    QTextStream out(&dataFile);
    out << QString("orientation: %1,%2").arg(ts).arg(o) << "\n";
}

void Collector::proximityChanged()
{
    const QProximityReading *proximityReading = proximity->reading();
    const quint64 ts = proximityReading->timestamp();
    const bool prox = proximityReading->close();

    QTextStream out(&dataFile);
    out << QString("proximity: %1,%2").arg(ts).arg(prox) << "\n";
}

void Collector::irProximityChanged()
{
    const QIRProximityReading *irProximityReading = irProx->reading();
    const quint64 ts = irProximityReading->timestamp();
    const qreal ref = irProximityReading->reflectance();

    QTextStream out(&dataFile);
    out << QString("irProximity: %1,%2").arg(ts).arg(ref) << "\n";
}

void Collector::tapChanged()
{
    const QTapReading *tapReading = tapSensor->reading();
    const quint64 ts = tapReading->timestamp();
    const bool dTap = tapReading->isDoubleTap();

    QTextStream out(&dataFile);
    out << QString("tap: %1,%2").arg(ts).arg(dTap) << "\n";
}

void Collector::startCollecting()
{
    if (dataFile.exists()) {
        fileCounter++;
        for (int i = 0; i < fileCounter; i++) {
            if (!QFileInfo(QString(QDir::tempPath()+"/sensordump_%1.dat").arg(fileCounter)).exists())
                dataFile.setFileName(QString(QDir::tempPath()+"/sensordump_%1.dat").arg(fileCounter));
            break;
            fileCounter++;
        }
    }
    if (!dataFile.exists()) {
        if (dataFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            accel->start();
            orientation->start();
            proximity->start();
            irProx->start();
            tapSensor->start();

            isActive = true;
        } else {
            qDebug() << "dump file not opened";
        }
    } else {
        startCollecting();
    }
}

void Collector::stopCollecting()
{
    if (isActive) {
        accel->stop();
        orientation->stop();
        proximity->stop();
        irProx->stop();
        tapSensor->stop();
        isActive = !isActive;
    }
    if (dataFile.isOpen())
        dataFile.close();
}


QML_DECLARE_TYPE(Collector)

