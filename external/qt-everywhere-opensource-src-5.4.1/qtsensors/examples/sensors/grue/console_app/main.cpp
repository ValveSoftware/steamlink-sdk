/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

#include <QtCore>
#include <qsensor.h>

class Filter : public QSensorFilter
{
    int lastPercent;
public:
    Filter()
        : QSensorFilter()
        , lastPercent(0)
    {
    }

    bool filter(QSensorReading *reading)
    {
        int percent = reading->property("chanceOfBeingEaten").value<int>();
        if (percent == 0) {
            qDebug() << "It is light. You are safe from Grues.";
        } else if (lastPercent == 0) {
            qDebug() << "It is dark. You are likely to be eaten by a Grue.";
        }
        if (percent == 100) {
            qDebug() << "You have been eaten by a Grue!";
            QCoreApplication::instance()->quit();
        } else if (percent)
            qDebug() << "Your chance of being eaten by a Grue:" << percent << "percent.";
        lastPercent = percent;
        return false;
    }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QSensor sensor("GrueSensor");

    Filter filter;
    sensor.addFilter(&filter);
    sensor.start();

    if (!sensor.isActive()) {
        qWarning("The Grue sensor didn't start. You're on your own!");
        return 1;
    }

    return app.exec();
}

