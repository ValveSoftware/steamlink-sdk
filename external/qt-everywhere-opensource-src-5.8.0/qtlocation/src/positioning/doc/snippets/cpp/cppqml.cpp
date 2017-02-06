/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QVariant>
#include <QtPositioning/QGeoAddress>
#include <QtPositioning/QGeoLocation>
#include <QtPositioning/QGeoCircle>
#include <QtPositioning/QGeoAreaMonitorSource>

void cppQmlInterface(QObject *qmlObject)
{
   //! [Address get]
    QGeoAddress geoAddress = qmlObject->property("address").value<QGeoAddress>();
    //! [Address get]

    //! [Address set]
    qmlObject->setProperty("address", QVariant::fromValue(geoAddress));
    //! [Address set]

    //! [Location get]
    QGeoLocation geoLocation = qmlObject->property("location").value<QGeoLocation>();
    //! [Location get]

    //! [Location set]
    qmlObject->setProperty("location", QVariant::fromValue(geoLocation));
    //! [Location set]
}

class MyClass : public QObject
{
    Q_OBJECT
//! [BigBen]
public:
    MyClass() : QObject()
    {
        QGeoAreaMonitorSource *monitor = QGeoAreaMonitorSource::createDefaultSource(this);
        if (monitor) {
            connect(monitor, SIGNAL(areaEntered(QGeoAreaMonitorInfo,QGeoPositionInfo)),
                    this, SLOT(areaEntered(QGeoAreaMonitorInfo,QGeoPositionInfo)));
            connect(monitor, SIGNAL(areaExited(QGeoAreaMonitorInfo,QGeoPositionInfo)),
                    this, SLOT(areaExited(QGeoAreaMonitorInfo,QGeoPositionInfo)));

            QGeoAreaMonitorInfo bigBen("Big Ben");
            QGeoCoordinate position(51.50104, -0.124632);
            bigBen.setArea(QGeoCircle(position, 100));

            monitor->startMonitoring(bigBen);

        } else {
            qDebug() << "Could not create default area monitor";
        }
    }

public Q_SLOTS:
    void areaEntered(const QGeoAreaMonitorInfo &mon, const QGeoPositionInfo &update)
    {
        Q_UNUSED(mon)

        qDebug() << "Now within 100 meters, current position is" << update.coordinate();
    }

    void areaExited(const QGeoAreaMonitorInfo &mon, const QGeoPositionInfo &update)
    {
        Q_UNUSED(mon)

        qDebug() << "No longer within 100 meters, current position is" << update.coordinate();
    }
//! [BigBen]
};
