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


#ifndef SENSORFWSENSORBASE_H
#define SENSORFWSENSORBASE_H

#include <QtSensors/qsensorbackend.h>
#include <sensormanagerinterface.h>
#include <abstractsensor_i.h>

#include <QtSensors/QAmbientLightSensor>
#include <QtSensors/QIRProximitySensor>
#include <QtSensors/QTapSensor>
#include <QtSensors/QProximitySensor>

class SensorfwSensorBase : public QSensorBackend
{
    Q_OBJECT
public:
    SensorfwSensorBase(QSensor *sensor);
    virtual ~SensorfwSensorBase();


protected:
    virtual bool doConnect()=0;
    void start() override;
    void stop() override;

    static const float GRAVITY_EARTH;
    static const float GRAVITY_EARTH_THOUSANDTH;    //for speed
    static const int KErrNotFound;
    static const int KErrInUse;
    static QStringList m_bufferingSensors;

    void setRanges(qreal correctionFactor=1);
    virtual QString sensorName() const=0;

    template<typename T>
    void initSensor(bool &initDone)
    {
        const QString name = sensorName();

        if (!initDone) {
            if (!m_remoteSensorManager) {
                qDebug() << "There is no sensor manager yet, do not initialize" << name;
                return;
            }
            if (!m_remoteSensorManager->loadPlugin(name)) {
                sensorError(KErrNotFound);
                return;
            }
            m_remoteSensorManager->registerSensorInterface<T>(name);
        }
        m_sensorInterface = T::controlInterface(name);
        if (!m_sensorInterface) {
            m_sensorInterface = const_cast<T*>(T::listenInterface(name));
        }
        initDone = initSensorInterface(name);
    };


    AbstractSensorChannelInterface* m_sensorInterface;
    int m_bufferSize;
    int bufferSize() const;
    virtual qreal correctionFactor() const;
    bool reinitIsNeeded;
    bool isFeatureSupported(QSensor::Feature feature) const;

private:
    bool initSensorInterface(QString const &);
    static SensorManagerInterface* m_remoteSensorManager;
    int m_prevOutputRange;
    bool doConnectAfterCheck();
    int m_efficientBufferSize, m_maxBufferSize;

    QDBusServiceWatcher *watcher;
    bool m_available;
    bool running;
    bool m_attemptRestart;
private slots:
    void connectToSensord();
    void sensordUnregistered();
    void standyOverrideChanged();
};

#endif
