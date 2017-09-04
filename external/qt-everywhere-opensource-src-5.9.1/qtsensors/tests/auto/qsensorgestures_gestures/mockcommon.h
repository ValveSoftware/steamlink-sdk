/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef MOCKCOMMON_H
#define MOCKCOMMON_H

#include <qsensorbackend.h>

#include <QSensor>
#include <QTimer>
#include <QAccelerometer>
#include <QOrientationSensor>
#include <QIRProximitySensor>
#include <QProximitySensor>
#include <QTapSensor>

#include <QFile>
#include <QTextStream>

class mockcommonPrivate : public QObject
{
    Q_OBJECT
public:

    mockcommonPrivate();

    static mockcommonPrivate *instance();

    bool setFile(const QString &);
    bool parseData(const QString &line);
    QTimer *readTimer;

public slots:
    void timerout();

Q_SIGNALS:
    void accelData(const QString &data);
    void irProxyData(const QString &data);
    void orientData(const QString &data);
    void tapData(const QString &data);
    void proxyData(const QString &data);

private:
    QFile pFile;
    qreal oldAccelTs;
    qreal prevts;
    bool firstRun;
};

class mockcommon : public QSensorBackend
{
    Q_OBJECT
public:
    mockcommon(QSensor *sensor);

    void start();
    void stop();
    static char const * const id;

Q_SIGNALS:
    void parseAccelData(const QString &data);
    void parseIrProxyDatata(const QString &data);
    void parseOrientData(const QString &data);
    void parseTapData(const QString &data);
    void parseProxyData(const QString &data);

private:
   int m_timerid;
   friend class mockcommonPrivate;
   QTimer *timer;
   QSensor *parentSensor;

};

class mockaccelerometer : public mockcommon
{
    Q_OBJECT

public:
    static char const * const id;

    mockaccelerometer(QSensor *sensor);

public slots:
    void parseAccelData(const QString &data);

private:
    QAccelerometerReading m_reading;
    qreal lastTimestamp;
};

class mockorientationsensor : public mockcommon
{
    Q_OBJECT

public:
    static char const * const id;

    mockorientationsensor(QSensor *sensor);
public slots:

    void parseOrientData(const QString &data);

private:
    QOrientationReading m_reading;
};

class mockirproximitysensor : public mockcommon
{
    Q_OBJECT

public:
    static char const * const id;

    mockirproximitysensor(QSensor *sensor);
public slots:

    void parseIrProxyData(const QString &data);

private:
    QIRProximityReading m_reading;
};

class mocktapsensor : public mockcommon
{
    Q_OBJECT

public:
    static char const * const id;

    mocktapsensor(QSensor *sensor);
public slots:

    void parseTapData(const QString &data);

private:
    QTapReading m_reading;
};


class mockproximitysensor : public mockcommon
{
    Q_OBJECT

public:
    static char const * const id;

    mockproximitysensor(QSensor *sensor);
public slots:

    void parseProxyData(const QString &data);

private:
    QProximityReading m_reading;
};
#endif // MOCKCOMMON_H
