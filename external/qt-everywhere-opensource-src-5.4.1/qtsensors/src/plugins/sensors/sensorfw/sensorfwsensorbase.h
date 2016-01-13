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
    void start() Q_DECL_OVERRIDE;
    void stop() Q_DECL_OVERRIDE;

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

private:
    bool initSensorInterface(QString const &);
    static SensorManagerInterface* m_remoteSensorManager;
    int m_prevOutputRange;
    bool doConnectAfterCheck();
    int m_efficientBufferSize, m_maxBufferSize;

    QDBusServiceWatcher *watcher;
    bool m_available;
    bool running;
private slots:
    void connectToSensord();
    void sensordUnregistered();
};

#endif
