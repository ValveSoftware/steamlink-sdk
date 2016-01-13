/****************************************************************************
**
** Copyright (C) 2012 Research In Motion
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
#ifndef BBSENSORBACKEND_H
#define BBSENSORBACKEND_H

#include <qsensorbackend.h>
#include <QtCore/QFile>
#include <QtCore/QSocketNotifier>

// Earlier NDK versions did not ship sensor.h and the Playbook NDK still
// doesn't include it, that is why we have our own copy in here.
// We prefer the NDK version if that exists, as that is more up-to-date.
#ifdef HAVE_NDK_SENSOR_H
#include <sensor/sensor.h>
#include <devctl.h>
#include <errno.h>
#else
#include "sensor.h"
#endif

class BbGuiHelper;

class BbSensorBackendBase : public QSensorBackend
{
    Q_OBJECT

public:
    BbSensorBackendBase(const QString &devicePath, sensor_type_e sensorType, QSensor *sensor);

    void initSensorInfo();
    void setGuiHelper(BbGuiHelper *guiHelper);

    void start() Q_DECL_OVERRIDE;
    void stop() Q_DECL_OVERRIDE;
    bool isFeatureSupported(QSensor::Feature feature) const Q_DECL_OVERRIDE;

protected:
    BbGuiHelper *guiHelper() const;
    QFile& deviceFile();
    sensor_type_e sensorType() const;
    int orientationForRemapping() const;

    void setDevice(const QString &deviceFile, sensor_type_e sensorType);

    // This is called while the device file is open during initialization and gives a subclass
    // an opportunity to do additional initialization.
    virtual void additionalDeviceInit();

    // If true is returned here, initSensorInfo() will read the output range from the OS sensor
    // service and pass it to the QtSensor API.
    virtual bool addDefaultRange();

    // Converts a value from units of the OS sensor service to units needed by the QtSensors API.
    // This is used in initSensorInfo(), where the output range is read from the backend and passed
    // on to the QtSensors side.
    // One example is the magnetometer: The OS sensor service returns units in microtesla, whereas
    // QtSensors expects tesla. This function would therefore convert from microtesla to tesla.
    virtual qreal convertValue(float bbValue);

    bool isAutoAxisRemappingEnabled() const;

    // These functions will automatically remap the matrix or the axes if auto axes remapping is
    // enabled
    void remapMatrix(const float inputMatrix[3*3], float outputMatrix[3*3]);
    void remapAxes(float *x, float *y, float *z);

    virtual void processEvent(const sensor_event_t &sensorEvent) = 0;

private slots:
    void dataAvailable();
    void applyAlwaysOnProperty();
    void applyBuffering();
    bool setPaused(bool paused);
    void updatePauseState();
    void updateOrientation();

private:
    QFile m_deviceFile;
    QScopedPointer<QSocketNotifier> m_socketNotifier;
    sensor_type_e m_sensorType;
    BbGuiHelper *m_guiHelper;
    float m_mappingMatrix[4];
    bool m_started;
    bool m_applyingBufferSize;
};

template<class SensorReading>
class BbSensorBackend : public BbSensorBackendBase
{
public:
    BbSensorBackend(const QString &devicePath, sensor_type_e sensorType, QSensor *sensor)
        : BbSensorBackendBase(devicePath, sensorType, sensor)
    {
        setReading(&m_reading);
    }

protected:
    virtual bool updateReadingFromEvent(const sensor_event_t &sensorEvent, SensorReading *reading) = 0;

private:
    void processEvent(const sensor_event_t &sensorEvent)
    {
        // There may be "non-sensor" event types added later for housekeeping, so we have to check
        // if the senor type matches the expected value
        if (sensorEvent.type != sensorType())
            return;

        if (updateReadingFromEvent(sensorEvent, &m_reading)) {
            // The OS timestamp is in nanoseconds, QtSensors expects microseconds
            m_reading.setTimestamp(sensorEvent.timestamp / 1000);
            newReadingAvailable();
        }
    }

    SensorReading m_reading;
};

#endif
