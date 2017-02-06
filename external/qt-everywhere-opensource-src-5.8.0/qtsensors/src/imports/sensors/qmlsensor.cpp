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

#include "qmlsensor.h"
#include <QtSensors/QSensor>
#include <QDebug>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Sensor
    \instantiates QmlSensor
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \brief The Sensor element serves as a base type for sensors.

    The Sensor element serves as a base type for sensors.

    This element wraps the QSensor class. Please see the documentation for
    QSensor for details.

    This element cannot be directly created. Please use one of the sub-classes instead.
*/

QmlSensor::QmlSensor(QObject *parent)
    : QObject(parent)
    , m_parsed(false)
    , m_active(false)
    , m_reading(0)
{
}

QmlSensor::~QmlSensor()
{
}

/*!
    \qmlproperty string Sensor::identifier
    This property holds the backend identifier for the sensor.

    Please see QSensor::identifier for information about this property.
*/

QString QmlSensor::identifier() const
{
    return m_identifier;
}

void QmlSensor::setIdentifier(const QString &identifier)
{
    if (m_parsed) return;
    m_identifier = identifier;
    Q_EMIT identifierChanged();
}

/*!
    \qmlproperty string Sensor::type
    This property holds the type of the sensor.
*/

QString QmlSensor::type() const
{
    return QString::fromLatin1(sensor()->type());
}

/*!
    \qmlproperty bool Sensor::connectedToBackend
    This property holds a value indicating if the sensor has connected to a backend.

    Please see QSensor::connectedToBackend for information about this property.
*/

bool QmlSensor::isConnectedToBackend() const
{
    return sensor()->isConnectedToBackend();
}

/*!
    \qmlproperty bool Sensor::busy
    This property holds a value to indicate if the sensor is busy.

    Please see QSensor::busy for information about this property.
*/

bool QmlSensor::isBusy() const
{
    return sensor()->isBusy();
}

/*!
    \qmlproperty bool Sensor::active
    This property holds a value to indicate if the sensor is active.

    Please see QSensor::active for information about this property.
*/

void QmlSensor::setActive(bool active)
{
    m_active = active;
    if (!m_parsed) return; // delay (it'll get called again later)!
    bool wasActive = sensor()->isActive();
    if (wasActive == active) return;
    if (active) {
        sensor()->start();
        m_active = sensor()->isActive();
    } else {
        sensor()->stop();
    }
    if (m_active != wasActive)
        emit activeChanged();
}

bool QmlSensor::isActive() const
{
    return m_active;
}

/*!
    \qmlproperty bool Sensor::alwaysOn
    This property holds a value to indicate if the sensor should remain running when the screen is off.

    Please see QSensor::alwaysOn for information about this property.
*/

bool QmlSensor::isAlwaysOn() const
{
    return sensor()->isAlwaysOn();
}

void QmlSensor::setAlwaysOn(bool alwaysOn)
{
    sensor()->setAlwaysOn(alwaysOn);
}

/*!
    \qmlproperty bool Sensor::skipDuplicates
    \since QtSensors 5.1

    This property indicates whether duplicate reading values should be omitted.

    Please see QSensor::skipDuplicates for information about this property.
*/

bool QmlSensor::skipDuplicates() const
{
    return sensor()->skipDuplicates();
}

void QmlSensor::setSkipDuplicates(bool skipDuplicates)
{
    sensor()->setSkipDuplicates(skipDuplicates);
}

/*!
    \qmlproperty list<Range> Sensor::availableDataRates
    This property holds the data rates that the sensor supports.

    Please see QSensor::availableDataRates for information about this property.
*/

QQmlListProperty<QmlSensorRange> QmlSensor::availableDataRates() const
{
    QList<QmlSensorRange*> ret;
    ret.reserve(sensor()->availableDataRates().size());
    foreach (const qrange &r, sensor()->availableDataRates()) {
        QmlSensorRange *range = new QmlSensorRange;
        //QQmlEngine::setObjectOwnership(range, QQmlEngine::JavaScriptOwnership);
        range->setMinumum(r.first);
        range->setMaximum(r.second);
        ret << range;
    }
    return QQmlListProperty<QmlSensorRange>(const_cast<QmlSensor*>(this), ret);
}

/*!
    \qmlproperty int Sensor::dataRate
    This property holds the data rate that the sensor should be run at.

    Please see QSensor::dataRate for information about this property.
*/

int QmlSensor::dataRate() const
{
    return sensor()->dataRate();
}

void QmlSensor::setDataRate(int rate)
{
    if (rate != dataRate()) {
      sensor()->setDataRate(rate);
      Q_EMIT dataRateChanged();
    }
}

/*!
    \qmlproperty list<OutputRange> Sensor::outputRanges
    This property holds a list of output ranges the sensor supports.

    Please see QSensor::outputRanges for information about this property.
*/

QQmlListProperty<QmlSensorOutputRange> QmlSensor::outputRanges() const
{
    QList<QmlSensorOutputRange*> ret;
    ret.reserve(sensor()->outputRanges().size());
    foreach (const qoutputrange &r, sensor()->outputRanges()) {
        QmlSensorOutputRange *range = new QmlSensorOutputRange;
        //QQmlEngine::setObjectOwnership(range, QQmlEngine::JavaScriptOwnership);
        range->setMinimum(r.minimum);
        range->setMaximum(r.maximum);
        range->setAccuracy(r.accuracy);
        ret << range;
    }
    return QQmlListProperty<QmlSensorOutputRange>(const_cast<QmlSensor*>(this), ret);
}

/*!
    \qmlproperty int Sensor::outputRange
    This property holds the output range in use by the sensor.

    Please see QSensor::outputRange for information about this property.
*/

int QmlSensor::outputRange() const
{
    return sensor()->outputRange();
}

void QmlSensor::setOutputRange(int index)
{
    int oldRange = outputRange();
    if (oldRange == index) return;
    sensor()->setOutputRange(index);
    if (sensor()->outputRange() == index)
        Q_EMIT outputRangeChanged();
}

/*!
    \qmlproperty string Sensor::description
    This property holds a descriptive string for the sensor.
*/

QString QmlSensor::description() const
{
    return sensor()->description();
}

/*!
    \qmlproperty int Sensor::error
    This property holds the last error code set on the sensor.
*/

int QmlSensor::error() const
{
    return sensor()->error();
}

/*!
    \qmlproperty SensorReading Sensor::reading
    This property holds the reading class.

    Please see QSensor::reading for information about this property.
    \sa {QML Reading types}
*/

QmlSensorReading *QmlSensor::reading() const
{
    return m_reading;
}

/*!
    \qmlproperty Sensor::AxesOrientationMode Sensor::axesOrientationMode
    \since QtSensors 5.1
    This property holds the mode that affects how the screen orientation changes reading values.

    Please see QSensor::axesOrientationMode for information about this property.
*/

QmlSensor::AxesOrientationMode QmlSensor::axesOrientationMode() const
{
    return static_cast<QmlSensor::AxesOrientationMode>(sensor()->axesOrientationMode());
}

void QmlSensor::setAxesOrientationMode(QmlSensor::AxesOrientationMode axesOrientationMode)
{
    sensor()->setAxesOrientationMode(static_cast<QSensor::AxesOrientationMode>(axesOrientationMode));
}

/*!
    \qmlproperty int Sensor::currentOrientation
    \since QtSensors 5.1
    This property holds the current orientation that is used for rotating the reading values.

    Please see QSensor::currentOrientation for information about this property.
*/

int QmlSensor::currentOrientation() const
{
    return sensor()->currentOrientation();
}

/*!
    \qmlproperty int Sensor::userOrientation
    \since QtSensors 5.1
    This property holds the angle used for rotating the reading values in the UserOrientation mode.

    Please see QSensor::userOrientation for information about this property.
*/

int QmlSensor::userOrientation() const
{
    return sensor()->userOrientation();
}

void QmlSensor::setUserOrientation(int userOrientation)
{
    sensor()->setUserOrientation(userOrientation);
}

/*!
    \qmlproperty int Sensor::maxBufferSize
    \since QtSensors 5.1
    This property holds the maximum buffer size.

    Please see QSensor::maxBufferSize for information about this property.
*/

int QmlSensor::maxBufferSize() const
{
    return sensor()->maxBufferSize();
}

/*!
    \qmlproperty int Sensor::efficientBufferSize
    \since QtSensors 5.1
    The property holds the most efficient buffer size.

    Please see QSensor::efficientBufferSize for information about this property.
*/

int QmlSensor::efficientBufferSize() const
{
    return sensor()->efficientBufferSize();
}

/*!
    \qmlproperty int Sensor::bufferSize
    \since QtSensors 5.1
    This property holds the size of the buffer.

    Please see QSensor::bufferSize for information about this property.
*/

int QmlSensor::bufferSize() const
{
    return sensor()->bufferSize();
}

void QmlSensor::setBufferSize(int bufferSize)
{
    sensor()->setBufferSize(bufferSize);
}

/*!
    \qmlmethod bool Sensor::start()
    Start retrieving values from the sensor. Returns true if the sensor was started, false otherwise.

    Please see QSensor::start() for information.
*/

bool QmlSensor::start()
{
    setActive(true);
    return isActive();
}

/*!
    \qmlmethod bool Sensor::stop()
    Stop retrieving values from the sensor.

    Please see QSensor::stop() for information.
*/

void QmlSensor::stop()
{
    setActive(false);
}

void QmlSensor::classBegin()
{
}

void QmlSensor::componentComplete()
{
    m_parsed = true;

    connect(sensor(), SIGNAL(sensorError(int)), this, SIGNAL(errorChanged()));
    connect(sensor(), SIGNAL(activeChanged()), this, SIGNAL(activeChanged()));
    connect(sensor(), SIGNAL(alwaysOnChanged()), this, SIGNAL(alwaysOnChanged()));
    connect(sensor(), SIGNAL(skipDuplicatesChanged(bool)), this, SIGNAL(skipDuplicatesChanged(bool)));
    connect(sensor(), SIGNAL(axesOrientationModeChanged(AxesOrientationMode)),
            this, SIGNAL(axesOrientationModeChanged(AxesOrientationMode)));
    connect(sensor(), SIGNAL(userOrientationChanged(int)), this, SIGNAL(userOrientationChanged(int)));
    connect(sensor(), SIGNAL(currentOrientationChanged(int)), this, SIGNAL(currentOrientationChanged(int)));
    connect(sensor(), SIGNAL(bufferSizeChanged(int)), this, SIGNAL(bufferSizeChanged(int)));
    connect(sensor(), SIGNAL(maxBufferSizeChanged(int)), this, SIGNAL(maxBufferSizeChanged(int)));
    connect(sensor(), SIGNAL(efficientBufferSizeChanged(int)), this, SIGNAL(efficientBufferSizeChanged(int)));

    // We need to set this on the sensor object now
    sensor()->setIdentifier(m_identifier.toLocal8Bit());

    // These can change!
    QByteArray oldIdentifier = sensor()->identifier();
    int oldDataRate = dataRate();
    int oldOutputRange = outputRange();

    bool ok = sensor()->connectToBackend();
    if (ok) {
        Q_EMIT connectedToBackendChanged();
        m_reading = createReading();
        m_reading->setParent(this);
    }

    if (oldIdentifier != sensor()->identifier()) {
        m_identifier = QString::fromLatin1(sensor()->identifier());
        Q_EMIT identifierChanged();
    }
    if (oldDataRate != dataRate())
        Q_EMIT dataRateChanged();
    if (oldOutputRange != outputRange())
        Q_EMIT outputRangeChanged();

    // meta-data should become non-empty
    if (!description().isEmpty())
        Q_EMIT descriptionChanged();
    if (sensor()->availableDataRates().count())
        Q_EMIT availableDataRatesChanged();
    if (sensor()->outputRanges().count())
        Q_EMIT outputRangesChanged();

    _update();

    connect(sensor(), SIGNAL(readingChanged()), this, SLOT(updateReading()));
    if (m_active) {
        m_active = false;
        start();
    }
}

void QmlSensor::_update()
{
}

void QmlSensor::updateReading()
{
    if (m_reading) {
        m_reading->update();
        Q_EMIT readingChanged();
    }
}

/*!
    \qmltype SensorReading
    \instantiates QmlSensorReading
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \brief The SensorReading element serves as a base type for sensor readings.

    The SensorReading element serves as a base type for sensor readings.

    This element wraps the QSensorReading class. Please see the documentation for
    QSensorReading for details.

    This element cannot be directly created.
*/

QmlSensorReading::QmlSensorReading(QSensor *)
    : QObject(0)
{
}

QmlSensorReading::~QmlSensorReading()
{
}

/*!
    \qmlproperty quint64 SensorReading::timestamp
    A timestamp for the reading.

    Please see QSensorReading::timestamp for information about this property.
*/

quint64 QmlSensorReading::timestamp() const
{
    return m_timestamp;
}

void QmlSensorReading::update()
{
    quint64 ts = reading()->timestamp();
    if (m_timestamp != ts) {
        m_timestamp = ts;
        Q_EMIT timestampChanged();
    }
    readingUpdate();
}

QT_END_NAMESPACE
