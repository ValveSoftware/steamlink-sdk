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

#include "simulatorcommon.h"
#include "qsensordata_simulator_p.h"
#include <QtSimulator/QtSimulator>
#include <QDebug>
#include <QThread>

using namespace Simulator;

Q_GLOBAL_STATIC(SensorsConnection, sensorsConnection)

class SimulatorAsyncConnection: public QThread
{
    Q_OBJECT
public:
    SimulatorAsyncConnection()
        : QThread()
    {
        QtMobility::qt_registerSensorTypes();

        moveToThread(this);
        connect(this, SIGNAL(queueConnectToServer()),
                this, SLOT(doConnectToServer()),
                Qt::QueuedConnection);

        start();
    }

    ~SimulatorAsyncConnection()
    {
        quit();
        wait();
    }

    void connectToServer()
    {
        emit queueConnectToServer();
    }

signals:
    void queueConnectToServer();

    void initialSensorsDataSent();
    void connectionFailed();
    void setAvailableFeatures(quint32 features);
    void setAmbientLightData(const QtMobility::QAmbientLightReadingData &);
    void setLightData(const QtMobility::QLightReadingData &);
    void setAccelerometerData(const QtMobility::QAccelerometerReadingData &);
    void setMagnetometerData(const QtMobility::QMagnetometerReadingData &);
    void setCompassData(const QtMobility::QCompassReadingData &);
    void setProximityData(const QtMobility::QProximityReadingData &);
    void setIRProximityData(const QtMobility::QIRProximityReadingData &);

private slots:
    void doConnectToServer()
    {
        mConnection.reset(new Connection(Connection::Client, "QtSimulator_Mobility_ServerName1.3.0.0",
                                         0xbeef+1, Version(1,0,0,0), this));
        mWorker.reset(mConnection->connectToServer(Connection::simulatorHostName(true), 0xbeef+1));
        if (!mWorker) {
            qWarning("QtSensors simulator backend could not connect to the simulator!");
            emit connectionFailed();
            return;
        }
        mWorker->addReceiver(this);
        mWorker->call("setRequestsSensors");
    }

private:
    QScopedPointer<Simulator::Connection> mConnection;
    QScopedPointer<Simulator::ConnectionWorker> mWorker;
};

SensorsConnection::SensorsConnection(QObject *parent)
    : QObject(parent)
    , mInitialDataSent(false)
    , mConnectionFailed(false)
{
    mConnection = new SimulatorAsyncConnection();

    connect(mConnection, SIGNAL(initialSensorsDataSent()),
            this, SLOT(initialSensorsDataSent()));
    connect(mConnection, SIGNAL(connectionFailed()),
            this, SLOT(slotConnectionFailed()));
    connect(mConnection, SIGNAL(setAvailableFeatures(quint32)),
            this, SIGNAL(setAvailableFeatures(quint32)));
    connect(mConnection, SIGNAL(setAmbientLightData(QtMobility::QAmbientLightReadingData)),
            this, SLOT(setAmbientLightData(QtMobility::QAmbientLightReadingData)));
    connect(mConnection, SIGNAL(setLightData(QtMobility::QLightReadingData)),
            this, SLOT(setLightData(QtMobility::QLightReadingData)));
    connect(mConnection, SIGNAL(setAccelerometerData(QtMobility::QAccelerometerReadingData)),
            this, SLOT(setAccelerometerData(QtMobility::QAccelerometerReadingData)));
    connect(mConnection, SIGNAL(setMagnetometerData(QtMobility::QMagnetometerReadingData)),
            this, SLOT(setMagnetometerData(QtMobility::QMagnetometerReadingData)));
    connect(mConnection, SIGNAL(setCompassData(QtMobility::QCompassReadingData)),
            this, SLOT(setCompassData(QtMobility::QCompassReadingData)));
    connect(mConnection, SIGNAL(setProximityData(QtMobility::QProximityReadingData)),
            this, SLOT(setProximityData(QtMobility::QProximityReadingData)));
    connect(mConnection, SIGNAL(setIRProximityData(QtMobility::QIRProximityReadingData)),
            this, SLOT(setIRProximityData(QtMobility::QIRProximityReadingData)));

    mConnection->connectToServer();
}

SensorsConnection::~SensorsConnection()
{
    delete mConnection;
}

SensorsConnection *SensorsConnection::instance()
{
    SensorsConnection *connection = sensorsConnection();
    // It's safe to return 0 because this is checked when used
    //if (!connection) qFatal("Cannot return from SensorsConnection::instance because sensorsConnection() returned 0");
    return connection;
}

void SensorsConnection::setAmbientLightData(const QtMobility::QAmbientLightReadingData &data)
{
    qtAmbientLightData = data;
}

void SensorsConnection::setLightData(const QtMobility::QLightReadingData &data)
{
    qtLightData = data;
}

void SensorsConnection::setAccelerometerData(const QtMobility::QAccelerometerReadingData &data)
{
    qtAccelerometerData = data;
}

void SensorsConnection::setMagnetometerData(const QtMobility::QMagnetometerReadingData &data)
{
    qtMagnetometerData = data;
}

void SensorsConnection::setCompassData(const QtMobility::QCompassReadingData &data)
{
    qtCompassData = data;
}

void SensorsConnection::setProximityData(const QtMobility::QProximityReadingData &data)
{
    qtProximityData = data;
}

void SensorsConnection::setIRProximityData(const QtMobility::QIRProximityReadingData &data)
{
    qtIRProximityData = data;
}

void SensorsConnection::initialSensorsDataSent()
{
    mInitialDataSent = true;
}

void SensorsConnection::slotConnectionFailed()
{
    mInitialDataSent = false;
    mConnectionFailed = true;
}

SimulatorCommon::SimulatorCommon(QSensor *sensor)
    : QSensorBackend(sensor)
    , m_timerid(0)
{
    addDataRate(1, 100);
    sensor->setDataRate(20);
    (void)SensorsConnection::instance(); // Ensure this exists
}

void SimulatorCommon::start()
{
    SensorsConnection *connection = SensorsConnection::instance();
    if (!connection) {
        sensorStopped();
        return;
    }

    if (m_timerid)
        return;

    int rate = sensor()->dataRate();
    if (rate == 0)
        rate = 20;
    int interval = 1000 / rate;
    if (interval < 0)
        interval = 1000;

    if (interval)
        m_timerid = startTimer(interval);
}

void SimulatorCommon::stop()
{
    if (m_timerid) {
        killTimer(m_timerid);
        m_timerid = 0;
    }
}

void SimulatorCommon::timerEvent(QTimerEvent * /*event*/)
{
    SensorsConnection *connection = SensorsConnection::instance();
    if (!connection || connection->connectionFailed()) {
        stop();
        sensorStopped();
    }
    if (!connection->safe()) return; // wait until it's safe to read the data
    poll();
}

#include "simulatorcommon.moc"
