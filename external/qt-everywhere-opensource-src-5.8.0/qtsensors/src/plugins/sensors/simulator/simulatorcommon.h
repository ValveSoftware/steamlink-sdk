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

#ifndef SIMULATORCOMMON_H
#define SIMULATORCOMMON_H

#include <qsensorbackend.h>
#include "qsensordata_simulator_p.h"

class QTimer;

class SimulatorAsyncConnection;

class SensorsConnection : public QObject
{
    Q_OBJECT
public:
    explicit SensorsConnection(QObject *parent = 0);
    virtual ~SensorsConnection();

    static SensorsConnection *instance();
    bool safe() const { return mInitialDataSent; }
    bool connectionFailed() const { return mConnectionFailed; }

signals:
    void setAvailableFeatures(quint32 features);

public slots:
    void setAmbientLightData(const QtMobility::QAmbientLightReadingData &);
    void setLightData(const QtMobility::QLightReadingData &);
    void setAccelerometerData(const QtMobility::QAccelerometerReadingData &);
    void setMagnetometerData(const QtMobility::QMagnetometerReadingData &);
    void setCompassData(const QtMobility::QCompassReadingData &);
    void setProximityData(const QtMobility::QProximityReadingData &);
    void setIRProximityData(const QtMobility::QIRProximityReadingData &);
    void initialSensorsDataSent();
    void slotConnectionFailed();

private:
    SimulatorAsyncConnection *mConnection;
    bool mInitialDataSent;
    bool mConnectionFailed;

public:
    QtMobility::QAmbientLightReadingData qtAmbientLightData;
    QtMobility::QLightReadingData qtLightData;
    QtMobility::QAccelerometerReadingData qtAccelerometerData;
    QtMobility::QMagnetometerReadingData qtMagnetometerData;
    QtMobility::QCompassReadingData qtCompassData;
    QtMobility::QProximityReadingData qtProximityData;
    QtMobility::QIRProximityReadingData qtIRProximityData;
};

class SimulatorCommon : public QSensorBackend
{
public:
    SimulatorCommon(QSensor *sensor);

    void start() Q_DECL_OVERRIDE;
    void stop() Q_DECL_OVERRIDE;
    virtual void poll() = 0;
    void timerEvent(QTimerEvent * /*event*/) Q_DECL_OVERRIDE;

private:
    int m_timerid;
};

#endif

