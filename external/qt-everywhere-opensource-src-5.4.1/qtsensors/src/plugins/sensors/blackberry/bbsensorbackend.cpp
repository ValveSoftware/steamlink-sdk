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
#include "bbsensorbackend.h"

#include "bbguihelper.h"
#include <QtCore/QDebug>
#include <QtCore/qmath.h>
#include <fcntl.h>

static const int microSecondsPerSecond = 1000 * 1000;
static const int defaultBufferSize = 10;

static int microSecondsToHertz(uint microSeconds)
{
    return microSecondsPerSecond / microSeconds;
}

static uint hertzToMicroSeconds(int hertz)
{
    return microSecondsPerSecond / hertz;
}

static void remapMatrix(const float inputMatrix[3*3],
                        const float mappingMatrix[4],
                        float outputMatrix[3*3])
{
    int i,j,k;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 2; j++) { //only goto 2 because last column stays unchanged

            outputMatrix[i*3+j] = 0;

            for (k = 0; k < 2; k++) { //only goto 2 because we know rotation matrix is zero in bottom row
                outputMatrix[i*3+j] += inputMatrix[i*3+k] * mappingMatrix[k*2+j];
            }
        }

        outputMatrix[i*3+2] = inputMatrix[i*3+2];
    }
}

BbSensorBackendBase::BbSensorBackendBase(const QString &devicePath, sensor_type_e sensorType,
                                         QSensor *sensor)
    : QSensorBackend(sensor), m_deviceFile(devicePath), m_sensorType(sensorType), m_guiHelper(0),
      m_started(false), m_applyingBufferSize(false)
{
    m_mappingMatrix[0] = m_mappingMatrix[3] = 1;
    m_mappingMatrix[1] = m_mappingMatrix[2] = 0;
    connect(sensor, SIGNAL(alwaysOnChanged()), this, SLOT(applyAlwaysOnProperty()));
    connect(sensor, SIGNAL(bufferSizeChanged(int)), this, SLOT(applyBuffering()));
    connect(sensor, SIGNAL(userOrientationChanged(int)), this, SLOT(updateOrientation()));

    // Set some sensible default values
    sensor->setEfficientBufferSize(defaultBufferSize);
    sensor->setMaxBufferSize(defaultBufferSize);
}

BbGuiHelper *BbSensorBackendBase::guiHelper() const
{
    return m_guiHelper;
}

QFile &BbSensorBackendBase::deviceFile()
{
    return m_deviceFile;
}

sensor_type_e BbSensorBackendBase::sensorType() const
{
    return m_sensorType;
}

void BbSensorBackendBase::setDevice(const QString &deviceFile, sensor_type_e sensorType)
{
    if (deviceFile != m_deviceFile.fileName()) {
        setPaused(true);
        delete m_socketNotifier.take();
        m_deviceFile.close();

        m_sensorType = sensorType;
        m_deviceFile.setFileName(deviceFile);
        initSensorInfo();
        if (m_started)
            start();    // restart with new device file
    }
}

void BbSensorBackendBase::initSensorInfo()
{
    if (!m_deviceFile.open(QFile::ReadOnly | QFile::Unbuffered)) {
        qDebug() << "Failed to open sensor" << m_deviceFile.fileName()
                 << ":" << m_deviceFile.errorString();
    } else {

        // Explicitly switch to non-blocking mode, otherwise read() will wait until new sensor
        // data is available, and we have no way to check if there is more data or not (bytesAvailable()
        // does not work for unbuffered mode)
        const int oldFlags = fcntl(m_deviceFile.handle(), F_GETFL);
        if (fcntl(m_deviceFile.handle(), F_SETFL, oldFlags | O_NONBLOCK) == -1) {
            perror(QString::fromLatin1("Starting sensor %1 failed, fcntl() returned -1")
                        .arg(m_deviceFile.fileName()).toLocal8Bit());
            sensorError(errno);
            stop();
            return;
        }

        sensor_devctl_info_u deviceInfo;
        const int result = devctl(m_deviceFile.handle(), DCMD_SENSOR_INFO, &deviceInfo,
                                  sizeof(deviceInfo), NULL);
        if (result != EOK) {
            perror(QString::fromLatin1("Querying sensor info for %1 failed")
                        .arg(m_deviceFile.fileName()).toLocal8Bit());
        } else {
            if (addDefaultRange()) {
                addOutputRange(convertValue(deviceInfo.rx.info.range_min),
                               convertValue(deviceInfo.rx.info.range_max),
                               convertValue(deviceInfo.rx.info.resolution));
            }

            if (deviceInfo.rx.info.delay_max > 0 && deviceInfo.rx.info.delay_min > 0) {
                // Min and max intentionally swapped here, as the minimum delay is the maximum rate
                addDataRate(microSecondsToHertz(deviceInfo.rx.info.delay_max),
                            microSecondsToHertz(deviceInfo.rx.info.delay_min));
            }
        }
        additionalDeviceInit();

        // Instead of closing the device here and opening it again in start(), just pause the sensor.
        // This avoids an expensive close() and open() call.
        setPaused(true);

        m_socketNotifier.reset(new QSocketNotifier(m_deviceFile.handle(), QSocketNotifier::Read));
        m_socketNotifier->setEnabled(false);
        connect(m_socketNotifier.data(), SIGNAL(activated(int)), this, SLOT(dataAvailable()));
    }
}

void BbSensorBackendBase::setGuiHelper(BbGuiHelper *guiHelper)
{
    Q_ASSERT(!m_guiHelper);
    m_guiHelper = guiHelper;
    connect(m_guiHelper, SIGNAL(applicationActiveChanged()), this, SLOT(updatePauseState()));
    connect(guiHelper, SIGNAL(orientationChanged()), this, SLOT(updateOrientation()));
    updateOrientation();
}

void BbSensorBackendBase::additionalDeviceInit()
{
}

bool BbSensorBackendBase::addDefaultRange()
{
    return true;
}

qreal BbSensorBackendBase::convertValue(float bbValue)
{
    return bbValue;
}

bool BbSensorBackendBase::isAutoAxisRemappingEnabled() const
{
    return sensor()->isFeatureSupported(QSensor::AxesOrientation) &&
           sensor()->axesOrientationMode() != QSensor::FixedOrientation;
}

void BbSensorBackendBase::remapMatrix(const float inputMatrix[], float outputMatrix[])
{
    if (!isAutoAxisRemappingEnabled() || orientationForRemapping() == 0) {
        memcpy(outputMatrix, inputMatrix, sizeof(float) * 9);
        return;
    }

    ::remapMatrix(inputMatrix, m_mappingMatrix, outputMatrix);
}

void BbSensorBackendBase::remapAxes(float *x, float *y, float *z)
{
    Q_UNUSED(z);
    Q_ASSERT(x && y);
    if (!isAutoAxisRemappingEnabled() || orientationForRemapping() == 0)
        return;

    const int angle = orientationForRemapping();

    const float oldX = *x;
    const float oldY = *y;

    switch (angle) {
    case 90:
        *x = -oldY;
        *y = oldX;
    break;
    case 180:
        *x = -oldX;
        *y = -oldY;
    break;
    case 270:
        *x = oldY;
        *y = -oldX;
    break;
    }
}

void BbSensorBackendBase::start()
{
    Q_ASSERT(m_guiHelper);

    if (!m_deviceFile.isOpen() || !setPaused(false)) {
        qDebug() << "Starting sensor" << m_deviceFile.fileName()
                 << "failed:" << m_deviceFile.errorString();
        sensorError(m_deviceFile.error());
        return;
    }
    m_started = true;

    const int rateInHertz = sensor()->dataRate();
    if (rateInHertz != 0) {
        const uint rateInMicroseconds = hertzToMicroSeconds(rateInHertz);
        sensor_devctl_rate_u deviceRate;
        deviceRate.tx.rate = rateInMicroseconds;
        const int result = devctl(m_deviceFile.handle(), DCMD_SENSOR_RATE, &deviceRate,
                                  sizeof(deviceRate), NULL);
        if (result != EOK) {
            sensor()->setDataRate(0);
            perror(QString::fromLatin1("Setting sensor rate for %1 failed")
                   .arg(m_deviceFile.fileName()).toLocal8Bit());
        } else {
            if (deviceRate.rx.rate > 0)
                sensor()->setDataRate(microSecondsToHertz(deviceRate.rx.rate));
            else
                sensor()->setDataRate(0);
        }
    }

    // Enable/disable duplicate skipping
    sensor_devctl_skipdupevent_u deviceSkip;
    deviceSkip.tx.enable = sensor()->skipDuplicates();
    const int result = devctl(deviceFile().handle(), DCMD_SENSOR_SKIPDUPEVENT, &deviceSkip,
                              sizeof(deviceSkip), NULL);
    if (result != EOK) {
        perror(QString::fromLatin1("Setting duplicate skipping for %1 failed")
               .arg(m_deviceFile.fileName()).toLocal8Bit());
    }

    applyBuffering();
    applyAlwaysOnProperty();
}

void BbSensorBackendBase::stop()
{
    setPaused(true);
    m_started = false;
}

bool BbSensorBackendBase::isFeatureSupported(QSensor::Feature feature) const
{
    switch (feature) {
    case QSensor::AxesOrientation:
        return (sensorType() == SENSOR_TYPE_ACCELEROMETER || sensorType() == SENSOR_TYPE_MAGNETOMETER ||
                sensorType() == SENSOR_TYPE_GYROSCOPE || sensorType() == SENSOR_TYPE_GRAVITY ||
                sensorType() == SENSOR_TYPE_LINEAR_ACCEL || sensorType() ==  SENSOR_TYPE_ROTATION_VECTOR ||
                sensorType() == SENSOR_TYPE_ROTATION_MATRIX || sensorType() == SENSOR_TYPE_AZIMUTH_PITCH_ROLL);
    case QSensor::AlwaysOn:
    case QSensor::Buffering:
    case QSensor::AccelerationMode:
    case QSensor::SkipDuplicates:
    case QSensor::PressureSensorTemperature:
        return true;
    case QSensor::GeoValues:
    case QSensor::Reserved:
    case QSensor::FieldOfView:
        break;
    }

    return false;
}

void BbSensorBackendBase::dataAvailable()
{
    if (!m_started) {
        // Spurious dataAvailable() call, drain the device file of data. We also disable
        // the socket notifier for this, so this is just added safety here.
        m_deviceFile.readAll();
        return;
    }

    Q_FOREVER {
        sensor_event_t event;
        const qint64 numBytes = m_deviceFile.read(reinterpret_cast<char *>(&event),
                                                  sizeof(sensor_event_t));
        if (numBytes == -1) {
            break;
        } else if (numBytes == sizeof(sensor_event_t)) {
            processEvent(event);
        } else {
            qDebug() << "Reading sensor event data for" << m_deviceFile.fileName()
                     << "failed (unexpected data size):" << m_deviceFile.errorString();
        }
    }
}

void BbSensorBackendBase::applyAlwaysOnProperty()
{
    if (!m_deviceFile.isOpen() || !m_started)
        return;

    sensor_devctl_bkgrnd_u bgState;
    bgState.tx.enable = sensor()->isAlwaysOn() ? 1 : 0;

    const int result = devctl(m_deviceFile.handle(), DCMD_SENSOR_BKGRND, &bgState, sizeof(bgState), NULL);
    if (result != EOK) {
        perror(QString::fromLatin1("Setting sensor always on for %1 failed")
               .arg(m_deviceFile.fileName()).toLocal8Bit());
    }

    // We might need to pause now
    updatePauseState();
}

void BbSensorBackendBase::applyBuffering()
{
    if (!m_deviceFile.isOpen() || !m_started || m_applyingBufferSize)
        return;

    // Flag to prevent recursion. We call setBufferSize() below, and because of the changed signal,
    // we might end up in this slot again.
    // The call to setBufferSize() is needed since the requested buffer size is most likely different
    // from the actual buffer size that will be used.
    m_applyingBufferSize = true;

    const bool enableBuffering = sensor()->bufferSize() > 1;
    sensor_devctl_queue_u queueControl;
    queueControl.tx.enable = enableBuffering ? 1 : 0;
    const int result = devctl(m_deviceFile.handle(), DCMD_SENSOR_QUEUE, &queueControl, sizeof(queueControl), NULL);
    if (result != EOK) {
        perror(QString::fromLatin1("Enabling sensor queuing for %1 failed")
               .arg(m_deviceFile.fileName()).toLocal8Bit());
    } else {
        if (enableBuffering) {
            int actualBufferSize = queueControl.rx.size;

            // Some firmware versions don't report the buffer size correctly. Simply pretend the
            // buffer size is the same as the requested buffer size, as setting the buffer size to
            // 1 here would seem as if buffering were disabled.
            if (actualBufferSize == 1)
                actualBufferSize = sensor()->bufferSize();

            sensor()->setBufferSize(actualBufferSize);
            sensor()->setEfficientBufferSize(actualBufferSize);
            sensor()->setMaxBufferSize(actualBufferSize);
        } else {
            sensor()->setBufferSize(1);
            sensor()->setEfficientBufferSize(defaultBufferSize);
            sensor()->setMaxBufferSize(defaultBufferSize);
        }
    }
    m_applyingBufferSize = false;
}

bool BbSensorBackendBase::setPaused(bool paused)
{
    if (!m_deviceFile.isOpen())
        return false;

    sensor_devctl_enable_u enableState;
    enableState.tx.enable = paused ? 0 : 1;

    if (m_socketNotifier)
        m_socketNotifier->setEnabled(!paused);

    const int result = devctl(m_deviceFile.handle(), DCMD_SENSOR_ENABLE, &enableState, sizeof(enableState), NULL);
    if (result != EOK) {
        perror(QString::fromLatin1("Setting sensor enabled (%1) for %2 failed")
               .arg(paused)
               .arg(m_deviceFile.fileName()).toLocal8Bit());
        return false;
    }

    return true;
}

void BbSensorBackendBase::updatePauseState()
{
    if (!m_started)
        return;

    setPaused(!sensor()->isAlwaysOn() && !m_guiHelper->applicationActive());
}

void BbSensorBackendBase::updateOrientation()
{
    const int rotationAngle = orientationForRemapping();

    m_mappingMatrix[0] = cos(rotationAngle*M_PI/180);
    m_mappingMatrix[1] = sin(rotationAngle*M_PI/180);
    m_mappingMatrix[2] = -sin(rotationAngle*M_PI/180);
    m_mappingMatrix[3] = cos(rotationAngle*M_PI/180);

    if (sensor()->isFeatureSupported(QSensor::AxesOrientation))
        sensor()->setCurrentOrientation(rotationAngle);
}

int BbSensorBackendBase::orientationForRemapping() const
{
    if (!sensor()->isFeatureSupported(QSensor::AxesOrientation))
        return 0;

    switch (sensor()->axesOrientationMode()) {
    default:
    case QSensor::FixedOrientation: return 0;
    case QSensor::AutomaticOrientation: return guiHelper()->currentOrientation();
    case QSensor::UserOrientation: return sensor()->userOrientation();
    }
}
