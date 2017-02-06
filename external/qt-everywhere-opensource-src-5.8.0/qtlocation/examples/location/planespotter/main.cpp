/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QObject>
#include <QTime>
#include <QBasicTimer>
#include <QDebug>
#include <QEasingCurve>
#include <QGeoCoordinate>
#include <QtPositioning/private/qgeoprojection_p.h>

#define ANIMATION_DURATION 4000

//! [PlaneController1]
class PlaneController: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QGeoCoordinate position READ position WRITE setPosition NOTIFY positionChanged)
//! [PlaneController1]
    //! [C++Pilot1]
    Q_PROPERTY(QGeoCoordinate from READ from WRITE setFrom NOTIFY fromChanged)
    Q_PROPERTY(QGeoCoordinate to READ to WRITE setTo NOTIFY toChanged)
    //! [C++Pilot1]

public:
    PlaneController()
    {
        easingCurve.setType(QEasingCurve::InOutQuad);
        easingCurve.setPeriod(ANIMATION_DURATION);
    }

    void setFrom(const QGeoCoordinate& from)
    {
        fromCoordinate = from;
    }

    QGeoCoordinate from() const
    {
        return fromCoordinate;
    }

    void setTo(const QGeoCoordinate& to)
    {
        toCoordinate = to;
    }

    QGeoCoordinate to() const
    {
        return toCoordinate;
    }

    void setPosition(const QGeoCoordinate &c) {
        if (currentPosition == c)
            return;

        currentPosition = c;
        emit positionChanged();
    }

    QGeoCoordinate position() const
    {
        return currentPosition;
    }

    Q_INVOKABLE bool isFlying() const {
        return timer.isActive();
    }

//! [C++Pilot2]
public slots:
    void startFlight()
    {
        if (timer.isActive())
            return;

        startTime = QTime::currentTime();
        finishTime = startTime.addMSecs(ANIMATION_DURATION);

        timer.start(15, this);
        emit departed();
    }
//! [C++Pilot2]

    void swapDestinations() {
        if (currentPosition == toCoordinate) {
            // swap destinations
            toCoordinate = fromCoordinate;
            fromCoordinate = currentPosition;
        }
    }

signals:
    void positionChanged();
    void arrived();
    void departed();
    void toChanged();
    void fromChanged();

protected:
    void timerEvent(QTimerEvent *event) Q_DECL_OVERRIDE
    {
        if (!event)
            return;

        if (event->timerId() == timer.timerId())
            updatePosition();
        else
            QObject::timerEvent(event);
    }

private:
    //! [C++Pilot3]
    void updatePosition()
    {
        // simple progress animation
        qreal progress;
        QTime current = QTime::currentTime();
        if (current >= finishTime) {
            progress = 1.0;
            timer.stop();
        } else {
            progress = ((qreal)startTime.msecsTo(current) / ANIMATION_DURATION);
        }

        setPosition(QGeoProjection::coordinateInterpolation(
                          fromCoordinate, toCoordinate, easingCurve.valueForProgress(progress)));

        if (!timer.isActive())
            emit arrived();
    }
    //! [C++Pilot3]

private:
    QGeoCoordinate currentPosition;
    QGeoCoordinate fromCoordinate, toCoordinate;
    QBasicTimer timer;
    QTime startTime, finishTime;
    QEasingCurve easingCurve;
//! [PlaneController2]
    // ...
};
//! [PlaneController2]

//! [PlaneControllerMain]
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    PlaneController oslo2berlin;
    PlaneController berlin2london;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("oslo2Berlin", &oslo2berlin);
    engine.rootContext()->setContextProperty("berlin2London", &berlin2london);
    engine.load(QUrl(QStringLiteral("qrc:/planespotter.qml")));

    return app.exec();
}
//! [PlaneControllerMain]

#include "main.moc"
