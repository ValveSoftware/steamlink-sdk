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

#include "qsensor.h"
#include "qsensor_p.h"
#include "qsensorbackend.h"
#include "qsensormanager.h"
#include <QDebug>
#include <QMetaProperty>
#include <QTimer>

QT_BEGIN_NAMESPACE

/*!
    \typedef qrange
    \relates QSensor
    \since 5.1

    This type is defined as a QPair.

    \code
    typedef QPair<int,int> qrange;
    \endcode

    \sa QPair, qrangelist, QSensor::availableDataRates
*/

/*!
    \typedef qrangelist
    \relates QSensor
    \since 5.1

    This type is defined as a list of qrange values.

    \code
    typedef QList<qrange> qrangelist;
    \endcode

    \sa QList, qrange, QSensor::availableDataRates
*/

/*!
    \class qoutputrange
    \relates QSensor
    \inmodule QtSensors
    \brief The qoutputrange class holds the specifics of an output range.
    \since 5.1

    The class is defined as a simple struct.

    \code
    struct qoutputrange
    {
        qreal maximum;
        qreal minimum;
        qreal accuracy;
    };
    \endcode

    Each output range specifies a minimum and maximum value as well as an accuracy value.
    The accuracy value represents the resolution of the sensor. It is the smallest change
    the sensor can detect and is expressed using the same units as the minimum and maximum.

    Sensors must often trade off range for accuracy. To allow the user to determine which of
    these are more important the sensor may offer several output ranges. One output
    range may have reduced minimum and maximum values and increased sensitivity. Another output
    range may have higher minimum and maximum values with reduced sensitivity. Note that higher
    sensitivities will be represented by smaller accuracy values.

    An example of this tradeoff can be seen by examining the LIS302DL accelerometer. It has only
    256 possible values to report with. These values are scaled so that they can represent either
    -2G to +2G (with an accuracy value of 0.015G) or -8G to +8G (with an accuracy value of 0.06G).

    \sa qoutputrangelist, QSensor::outputRanges
*/

/*!
    \variable qoutputrange::maximum

    This is the maximum value for this output range.
    The units are defined by the sensor.
*/

/*!
    \variable qoutputrange::minimum

    This is the minimum value for this output range.
    The units are defined by the sensor.
*/

/*!
    \variable qoutputrange::accuracy

    The accuracy value represents the resolution of the sensor. It is the smallest change
    the sensor can detect and is expressed using the same units as the minimum and maximum.
*/

/*!
    \typedef qoutputrangelist
    \relates QSensor
    \since 5.1

    This type is defined as a list of qoutputrange values.

    \code
    typedef QList<qoutputrange> qoutputrangelist;
    \endcode

    \sa QList, qoutputrange, QSensor::outputRanges
*/

static void registerTypes()
{
    qRegisterMetaType<qrange>("qrange");
    qRegisterMetaType<qrangelist>("qrangelist");
    qRegisterMetaType<qoutputrangelist>("qoutputrangelist");
}
Q_CONSTRUCTOR_FUNCTION(registerTypes)

// =====================================================================

void QSensorPrivate::init(const QByteArray &sensorType)
{
    Q_Q(QSensor);
    type = sensorType;
    q->registerInstance(); // so the availableSensorsChanged() signal works
}

/*!
    \class QSensor
    \ingroup sensors_main
    \inmodule QtSensors
    \since 5.1

    \brief The QSensor class represents a single hardware sensor.

    The life cycle of a sensor is typically:

    \list
    \li Create a sub-class of QSensor on the stack or heap.
    \li Setup as required by the application.
    \li Start receiving values.
    \li Sensor data is used by the application.
    \li Stop receiving values.
    \endlist

    The sensor data is delivered via QSensorReading and its sub-classes.

    \section1 Orientation

    Some sensors react to screen orientation changes, such as QAccelerometer, QMagnetometer and
    QRotationSensor. These are so called \e orientable sensors. For orientable sensors,
    QSensor supports changing the reporting of the reading values based on the orientation of the
    screen.

    For orientable sensors, the axesOrientationMode property controls how the orientation affects
    the reading values.

    In the default mode, QSensor::FixedOrientation, the reading values remain
    unaffected by the orientation. In the QSensor::AutomaticOrientation mode, the reading
    values are automatically rotated by taking the current screen orientation into account. And
    finally, in the QSensor::UserOrientation mode, the reading values are rotated
    according to a user-specified orientation.

    The functionality of this is only available if it is supported by the backend and if the sensor
    is orientable, which can be checked by calling QSensor::isFeatureSupported()
    with the QSensor::AxesOrientation flag.

    The orientation values here are always of the screen orientation, not the device orientation.
    The screen orientation is the orientation of the GUI. For example when rotating a device by 90
    degrees counter-clockwise, the screen orientation compensates for that by rotating 90 degrees
    clockwise, to the effect that the GUI is still facing upright after the device has been rotated.
    Note that applications can lock the screen orientation, for example to force portrait or landscape
    mode. For locked orientations, orientable sensors will not react with reading changes if the device
    orientation is changed, as orientable sensors react to screen orientation changes only. This makes
    sense, as the purpose of orientable sensors is to keep the sensor orientation in sync with the screen
    orientation.

    The orientation values range from 0 to 270 degrees. The orientation is applied in clockwise direction,
    e.g. an orientation value of 90 degrees means that the screen has been rotated 90 degress to the right
    from its origin position, to compensate a device rotation of 90 degrees to the left.

    \sa QSensorReading
*/

/*!
    \enum QSensor::Feature
    \brief Lists optional features a backend might support.

    The features common to all sensor types are:

    \value Buffering The backend supports buffering of readings, controlled by the
                     QSensor::bufferSize property.
    \value AlwaysOn The backend supports changing the policy on whether to suspend when idle,
                    controlled by the QSensor::alwaysOn property.
    \value SkipDuplicates The backend supports skipping of same or very similar successive
                          readings. This can be enabled by setting the QSensor::skipDuplicates
                          property to true.

    The features of QMagnetometer are:

    \value GeoValues The backend supports returning geo values, which can be
                     controlled with the QMagnetometer::returnGeoValues property.

    The features of QLightSensor are:

    \value FieldOfView The backend specifies its field of view, which can be
                       read from the QLightSensor::fieldOfView property.

    The features of QAccelerometer are:

    \value AccelerationMode The backend supports switching the acceleration mode
                            of the acceleromter with the QAccelerometer::accelerationMode property.

    The features of QPressureSensor are:

    \value PressureSensorTemperature The backend provides the pressure sensor's die temperature

    The features of all orientable sensors are:

    \value AxesOrientation The backend supports changing the axes orientation from the default of
                           QSensor::FixedOrientation to something else.

    \omitvalue Reserved

    \sa QSensor::isFeatureSupported()
    \since 5.0
*/

/*!
    Construct the \a type sensor as a child of \a parent.

    Do not use this constructor if a derived class exists for the specific sensor type.

    The wrong way is to use the base class constructor:
    \snippet sensors/creating.cpp 3
    The right way is to create an instance of the derived class:
    \snippet sensors/creating.cpp 2

    The derived classes have
    additional properties and data members which are needed for certain features such as
    geo value support in QMagnetometer or acceleration mode support in QAccelerometer.
    These features will only work properly when creating a sensor instance from a QSensor
    subclass.

    Only use this constructor if there is no derived sensor class available. Note that all
    built-in sensors have a derived class, so using this constructor should only be necessary
    when implementing custom sensors, like in the \l {Qt Sensors - Grue Sensor Example}{Grue sensor example}.
*/
QSensor::QSensor(const QByteArray &type, QObject *parent)
    : QObject(*new QSensorPrivate, parent)
{
    Q_D(QSensor);
    d->init(type);
}

/*! \internal
 */
QSensor::QSensor(const QByteArray &type, QSensorPrivate &dd, QObject* parent)
    : QObject(dd, parent)
{
    Q_D(QSensor);
    d->init(type);
}

/*! \internal
 */
QSensorBackend *QSensor::backend() const
{
    Q_D(const QSensor);
    return d->backend;
}

/*!
    Destroy the sensor. Stops the sensor if it has not already been stopped.
*/
QSensor::~QSensor()
{
    Q_D(QSensor);
    stop();
    Q_FOREACH (QSensorFilter *filter, d->filters)
        filter->setSensor(0);
    delete d->backend;
    d->backend = 0;
    // owned by the backend
    d->device_reading = 0;
    d->filter_reading = 0;
    d->cache_reading = 0;
}

/*!
    \property QSensor::connectedToBackend
    \brief a value indicating if the sensor has connected to a backend.

    A sensor that has not been connected to a backend cannot do anything useful.

    Call the connectToBackend() method to force the sensor to connect to a backend
    immediately. This is automatically called if you call start() so you only need
    to do this if you need access to sensor properties (ie. to poll the sensor's
    meta-data before you use it).
*/

bool QSensor::isConnectedToBackend() const
{
    Q_D(const QSensor);
    return (d->backend != 0);
}

/*!
    \property QSensor::identifier
    \brief the backend identifier for the sensor.

    Note that the identifier is filled out automatically
    when the sensor is connected to a backend. If you want
    to connect a specific backend, you should call
    setIdentifier() before connectToBackend().
*/

QByteArray QSensor::identifier() const
{
    Q_D(const QSensor);
    return d->identifier;
}

void QSensor::setIdentifier(const QByteArray &identifier)
{
    Q_D(QSensor);
    if (isConnectedToBackend()) {
        qWarning() << "ERROR: Cannot call QSensor::setIdentifier while connected to a backend!";
        return;
    }
    d->identifier = identifier;
}

/*!
    \property QSensor::type
    \brief the type of the sensor.
*/

QByteArray QSensor::type() const
{
    Q_D(const QSensor);
    return d->type;
}

/*!
    Try to connect to a sensor backend.

    Returns true if a suitable backend could be found, false otherwise.

    The type must be set before calling this method if you are using QSensor directly.

    \sa isConnectedToBackend()
*/
bool QSensor::connectToBackend()
{
    Q_D(QSensor);
    if (isConnectedToBackend())
        return true;

    int dataRate = d->dataRate;
    int outputRange = d->outputRange;

    d->backend = QSensorManager::createBackend(this);

    if (d->backend) {
        // Reset the properties to their default values and re-set them now so
        // that the logic we've put into the setters gets called.
        if (dataRate != 0) {
            d->dataRate = 0;
            setDataRate(dataRate);
        }
        if (outputRange != -1) {
            d->outputRange = -1;
            setOutputRange(outputRange);
        }
    }

    return isConnectedToBackend();
}

/*!
    \property QSensor::busy
    \brief a value to indicate if the sensor is busy.

    Some sensors may be on the system but unavailable for use.
    This function will return true if the sensor is busy. You
    will not be able to start() the sensor.

    Note that this function does not return true if you
    are using the sensor, only if another process is using
    the sensor.

    \sa busyChanged()
*/

bool QSensor::isBusy() const
{
    Q_D(const QSensor);
    return d->busy;
}

/*!
    \fn QSensor::busyChanged()

    This signal is emitted when the sensor is no longer busy.
    This can be used to grab a sensor when it becomes available.

    \code
    sensor.start();
    if (sensor.isBusy()) {
        // need to wait for busyChanged signal and try again
    }
    \endcode
*/

/*!
    \property QSensor::active
    \brief a value to indicate if the sensor is active.

    This is true if the sensor is active (returning values). This is false otherwise.

    Note that setting this value to true will not have an immediate effect. Instead,
    the sensor will be started once the event loop has been reached.
*/
void QSensor::setActive(bool active)
{
    if (active == isActive())
        return;

    if (active)
        QTimer::singleShot(0, this, SLOT(start())); // delay ensures all properties have been set if using QML
    else
        stop();
}

bool QSensor::isActive() const
{
    Q_D(const QSensor);
    return d->active;
}

/*!
    \property QSensor::alwaysOn
    \brief a value to indicate if the sensor should remain running when the screen is off.

    Some platforms have a policy of suspending sensors when the screen turns off.
    Setting this property to true will ensure the sensor continues to run.
*/
/*!
    \fn QSensor::alwaysOnChanged()

    This signal is emitted when the alwaysOn property changes.
*/
void QSensor::setAlwaysOn(bool alwaysOn)
{
    Q_D(QSensor);
    if (d->alwaysOn == alwaysOn) return;
    d->alwaysOn = alwaysOn;
    emit alwaysOnChanged();
}

bool QSensor::isAlwaysOn() const
{
    Q_D(const QSensor);
    return d->alwaysOn;
}

/*!
    \property QSensor::skipDuplicates
    \brief Indicates whether duplicate reading values should be omitted.
    \since 5.1

    When duplicate skipping is enabled, successive readings with the same or very
    similar values are omitted. This helps reducing the amount of processing done, as less sensor
    readings are made available. As a consequence, readings arrive at an irregular interval.

    Duplicate skipping is not just enabled for readings that are exactly the same, but also for
    readings that are quite similar, as each sensor has a bit of jitter even if the device is
    not moved.

    Support for this property depends on the backend. Use isFeatureSupported() to check if it is
    supported on the current platform.

    Duplicate skipping is disabled by default.

    Duplicate skipping takes effect when the sensor is started, changing the property while the
    sensor is active has no immediate effect.
*/
bool QSensor::skipDuplicates() const
{
    Q_D(const QSensor);
    return d->skipDuplicates;
}

/*!
    Sets the duplicate skipping to \a skipDuplicates.

    \since 5.1
*/
void QSensor::setSkipDuplicates(bool skipDuplicates)
{
    Q_D(QSensor);
    if (d->skipDuplicates != skipDuplicates) {
        d->skipDuplicates = skipDuplicates;
        emit skipDuplicatesChanged(skipDuplicates);
    }
}

/*!
    \fn QSensor::skipDuplicatesChanged(bool skipDuplicates)
    \since 5.1

    This signal is emitted when the skipDuplicates property changes.
*/

/*!
    \property QSensor::availableDataRates
    \brief the data rates that the sensor supports.

    This is a list of the data rates that the sensor supports.
    Measured in Hertz.

    Entries in the list can represent discrete rates or a
    continuous range of rates.
    A discrete rate is noted by having both values the same.

    See the sensor_explorer example for an example of how to interpret and use
    this information.

    Note that this information is not mandatory as not all sensors have a rate at which
    they run. In such cases, the list will be empty.

    \sa QSensor::dataRate, qrangelist
*/

qrangelist QSensor::availableDataRates() const
{
    Q_D(const QSensor);
    return d->availableDataRates;
}

/*!
    \property QSensor::dataRate
    \brief the data rate that the sensor should be run at.

    Measured in Hertz.

    The data rate is the maximum frequency at which the sensor can detect changes.

    Setting this property is not portable and can cause conflicts with other
    applications. Check with the sensor backend and platform documentation for
    any policy regarding multiple applications requesting a data rate.

    The default value (0) means that the app does not care what the data rate is.
    Applications should consider using a timer-based poll of the current value or
    ensure that the code that processes values can run very quickly as the platform
    may provide updates hundreds of times each second.

    This should be set before calling start() because the sensor may not
    notice changes to this value while it is running.

    Note that there is no mechanism to determine the current data rate in use by the
    platform.

    \sa QSensor::availableDataRates
*/

int QSensor::dataRate() const
{
    Q_D(const QSensor);
    return d->dataRate;
}

void QSensor::setDataRate(int rate)
{
    Q_D(QSensor);
    if (d->dataRate != rate) {
        d->dataRate = rate;
        emit dataRateChanged();
    }
}

/*!
   Checks if a specific feature is supported by the backend.

   QtSensors supports a rich API for controlling and providing information about sensors. Naturally,
   not all of this functionality can be supported by all of the backends.

   To check if the current backend supports the feature \a feature, call this function.

   The backend needs to be connected, otherwise false will be returned. Calling connectToBackend()
   or start() will create a connection to the backend.

   Backends have to implement QSensorBackend::isFeatureSupported() to make this work.

   Returns whether or not the feature is supported if the backend is connected, or false if the backend is not connected.
   \since 5.0
 */
bool QSensor::isFeatureSupported(Feature feature) const
{
    Q_D(const QSensor);
    return d->backend && d->backend->isFeatureSupported(feature);
}

/*!
    Start retrieving values from the sensor.
    Returns true if the sensor was started, false otherwise.

    The sensor may fail to start for several reasons.

    Once an application has started a sensor it must wait until the sensor receives a
    new value before it can query the sensor's values. This is due to how the sensor
    receives values from the system. Sensors do not (in general) poll for new values,
    rather new values are pushed to the sensors as they happen.

    For example, this code will not work as intended.

    \badcode
    sensor->start();
    sensor->reading()->x(); // no data available
    \endcode

    To work correctly, the code that accesses the reading should ensure the
    readingChanged() signal has been emitted.

    \code
        connect(sensor, SIGNAL(readingChanged()), this, SLOT(checkReading()));
        sensor->start();
    }
    void MyClass::checkReading() {
        sensor->reading()->x();
    \endcode

    \sa QSensor::busy
*/
bool QSensor::start()
{
    Q_D(QSensor);
    if (isActive())
        return true;
    if (!connectToBackend())
        return false;
    // Set these flags to their defaults
    d->active = true;
    d->busy = false;
    // Backend will update the flags appropriately
    d->backend->start();
    Q_EMIT activeChanged();
    return isActive();
}

/*!
    Stop retrieving values from the sensor.

    This releases the sensor so that other processes can use it.

    \sa QSensor::busy
*/
void QSensor::stop()
{
    Q_D(QSensor);
    if (!isConnectedToBackend() || !isActive())
        return;
    d->active = false;
    d->backend->stop();
    Q_EMIT activeChanged();
}

/*!
    \property QSensor::reading
    \brief the reading class.

    The reading class provides access to sensor readings. The reading object
    is a volatile cache of the most recent sensor reading that has been received
    so the application should process readings immediately or save the values
    somewhere for later processing.

    Note that this will return 0 until a sensor backend is connected to a backend.

    Also note that readings are not immediately available after start() is called.
    Applications must wait for the readingChanged() signal to be emitted.

    \sa isConnectedToBackend(), start()
*/

QSensorReading *QSensor::reading() const
{
    Q_D(const QSensor);
    return d->cache_reading;
}

/*!
    Add a \a filter to the sensor.

    The sensor does not take ownership of the filter.
    QSensorFilter will inform the sensor if it is destroyed.

    \sa QSensorFilter
*/
void QSensor::addFilter(QSensorFilter *filter)
{
    Q_D(QSensor);
    if (!filter) {
        qWarning() << "addFilter: passed a null filter!";
        return;
    }
    filter->setSensor(this);
    d->filters << filter;
}

/*!
    Remove \a filter from the sensor.

    \sa QSensorFilter
*/
void QSensor::removeFilter(QSensorFilter *filter)
{
    Q_D(QSensor);
    if (!filter) {
        qWarning() << "removeFilter: passed a null filter!";
        return;
    }
    d->filters.removeOne(filter);
    filter->setSensor(0);
}

/*!
    Returns the filters currently attached to the sensor.

    \sa QSensorFilter
*/
QList<QSensorFilter*> QSensor::filters() const
{
    Q_D(const QSensor);
    return d->filters;
}

/*!
    \fn QSensor::readingChanged()

    This signal is emitted when a new sensor reading is received.

    The sensor reading can be found in the QSensor::reading property. Note that the
    reading object is a volatile cache of the most recent sensor reading that has
    been received so the application should process the reading immediately or
    save the values somewhere for later processing.

    Before this signal has been emitted for the first time, the reading object will
    have uninitialized data.

    \sa start()
*/

/*!
    \fn QSensor::activeChanged()

    This signal is emitted when the QSensor::active property has changed.

    \sa QSensor::active
*/

/*!
    \property QSensor::outputRanges
    \brief a list of output ranges the sensor supports.

    A sensor may have more than one output range. Typically this is done
    to give a greater measurement range at the cost of lowering accuracy.

    Note that this information is not mandatory. This information is typically only
    available for sensors that have selectable output ranges (such as typical
    accelerometers).

    \sa QSensor::outputRange, qoutputrangelist
*/

qoutputrangelist QSensor::outputRanges() const
{
    Q_D(const QSensor);
    return d->outputRanges;
}

/*!
    \property QSensor::outputRange
    \brief the output range in use by the sensor.

    This value represents the index in the QSensor::outputRanges list to use.

    Setting this property is not portable and can cause conflicts with other
    applications. Check with the sensor backend and platform documentation for
    any policy regarding multiple applications requesting an output range.

    The default value (-1) means that the app does not care what the output range is.

    Note that there is no mechanism to determine the current output range in use by the
    platform.

    \sa QSensor::outputRanges
*/

int QSensor::outputRange() const
{
    Q_D(const QSensor);
    return d->outputRange;
}

void QSensor::setOutputRange(int index)
{
    Q_D(QSensor);
    if (index == -1 || !isConnectedToBackend()) {
        d->outputRange = index;
        return;
    }
    bool warn = true;
    if (index >= 0 && index < d->outputRanges.count()) {
        warn = false;
        d->outputRange = index;
    }
    if (warn) {
        qWarning() << "setOutputRange:" << index << "is not supported by the sensor.";
    }
}

/*!
    \property QSensor::description
    \brief a descriptive string for the sensor.
*/

QString QSensor::description() const
{
    Q_D(const QSensor);
    return d->description;
}

/*!
    \property QSensor::error
    \brief the last error code set on the sensor.

    Note that error codes are sensor-specific.
*/

int QSensor::error() const
{
    Q_D(const QSensor);
    return d->error;
}

/*!
    \enum QSensor::AxesOrientationMode
    \since 5.1

    Describes how reading values are affected by the screen orientation.

    \value FixedOrientation No automatic rotation is applied to the reading values.

    \value AutomaticOrientation The reading values are automatically rotated based on the screen
                                orientation.

    \value UserOrientation The reading values are rotated based on the angle of the userOrientation property.

    \sa QSensor::axesOrientationMode
*/

/*!
    \property QSensor::axesOrientationMode
    \since 5.1
    \brief The mode that affects how the screen orientation changes reading values.

    When set to FixedOrientation, which is the default mode, no automatic rotation is applied to
    the reading. This is the only mode available for backends that do not support the
    QSensor::AxesOrientation feature.

    When set to AutomaticOrientation, the reading values are automatically rotated when the
    screen orientation changes. In effect, the screen orientation is canceled out.

    As an example, assume the device is rotated by 180 degrees and therefore the screen orientation
    also is rotated by 180 degrees from the native orientation. Without automatic axes orientation,
    the reading values would now be changed: Both the X and the Y values would be negated, forcing
    an application developer to manually cancel out the negation in application code. Automatic
    axes orientation does this automatically, in this mode the X and Y values would be the same as
    with the default screen orientation.

    This automatic rotation of the axes is handy is some usecases, for example in a bubble level
    application that measures how level a surface is by looking at the X axis value of an
    accelerometer. When the device and screen orientation change by 90 degrees, an application
    developer does not need to change anything, he can continue using the X axis value even though
    the device is rotated. Without automatic axes orientation, the application developer would need
    to look at the Y values instead, thereby adding code to the application that reads from a
    different axis depending on the screen orientation.

    The UserOrientation mode is quite similar to AutomaticOrientation, only that the screen orientation
    is manually controlled instead of automatically determined. The angle of the userOrientation
    property is then used for rotating the reading values.

    Since the rotation of the reading values is based on the screen orientation, Z values will never
    change, as the Z axis is perpendicular to the screen.
    As screen orientation changes in 90 degree steps, rotating the reading values is also done in
    steps of 90 degrees.

    This property is only used for orientable sensors.
*/

QSensor::AxesOrientationMode QSensor::axesOrientationMode() const
{
    Q_D(const QSensor);
    return d->axesOrientationMode;
}

void QSensor::setAxesOrientationMode(QSensor::AxesOrientationMode axesOrientationMode)
{
    Q_D(QSensor);
    if (d->axesOrientationMode != axesOrientationMode) {
        d->axesOrientationMode = axesOrientationMode;
        emit axesOrientationModeChanged(axesOrientationMode);
    }
}

/*!
    \property QSensor::currentOrientation
    \since 5.1
    \brief The current orientation that is used for rotating the reading values.

    This might not be the same as the screen orientation. For example, in the FixedOrientation mode,
    the reading values are not rotated, and therefore the property is 0.

    In the UserOrientation mode, the readings are rotated based on the userOrientation property,
    and therefore this property is equal to the userOrientation property.

    In the AutomaticOrientation mode, the readings are rotated based on the screen orientation,
    and therefore this property will be equal to the current screen orientation.

    This property is set by the backend and only valid for orientable sensors.
*/

int QSensor::currentOrientation() const
{
    Q_D(const QSensor);
    return d->currentOrientation;
}

/*!
    \since 5.1
    Sets the current screen orientation to \a currentOrientation. This is to be called from the
    backend whenever the screen orientation or the userOrientation property changes.
*/
void QSensor::setCurrentOrientation(int currentOrientation)
{
    Q_D(QSensor);
    if (d->currentOrientation != currentOrientation) {
        d->currentOrientation = currentOrientation;
        emit currentOrientationChanged(currentOrientation);
    }
}

/*!
    \property QSensor::userOrientation
    \since 5.1
    \brief The angle used for rotating the reading values in the UserOrientation mode.

    When the axesOrientationMode property is set to UserOrientation, the angle for rotating the
    reading values is taken from this property. In other modes, the property has no effect.

    The default is 0. The only valid values are 0, 90, 180 and 270, as those are the only possible
    screen orientations.

    This property is only valid for orientable sensors.
*/

int QSensor::userOrientation() const
{
    Q_D(const QSensor);
    return d->userOrientation;
}

void QSensor::setUserOrientation(int userOrientation)
{
    Q_D(QSensor);
    if (d->userOrientation != userOrientation) {
        d->userOrientation = userOrientation;
        emit userOrientationChanged(userOrientation);
    }
}

/*!
    \fn QSensor::sensorError(int error)

    This signal is emitted when an \a error code is set on the sensor.
    Note that some errors will cause the sensor to stop working.
    You should call isActive() to determine if the sensor is still running.
*/

/*!
    \fn QSensor::availableSensorsChanged()

    This signal is emitted when the list of available sensors has changed.
    The sensors available to a program will not generally change over time
    however some of the available sensors may represent hardware that is not
    permanently connected. For example, a game controller that is connected
    via bluetooth would become available when it was on and would become
    unavailable when it was off.

    \sa QSensor::sensorTypes(), QSensor::sensorsForType()
*/

/*!
    \property QSensor::maxBufferSize

    The property holds the maximum buffer size.

    Note that this may be 1, in which case the sensor does not support any form of buffering.
    In that case, isFeatureSupported(QSensor::Buffering) will also return false.

    \sa QSensor::bufferSize, QSensor::efficientBufferSize
*/

int QSensor::maxBufferSize() const
{
    Q_D(const QSensor);
    return d->maxBufferSize;
}

/*!
    \since 5.1
    Sets the maximum buffer size to \a maxBufferSize. This is to be called from the
    backend.
*/
void QSensor::setMaxBufferSize(int maxBufferSize)
{
    Q_D(QSensor);
    if (d->maxBufferSize != maxBufferSize) {
        d->maxBufferSize = maxBufferSize;
        emit maxBufferSizeChanged(maxBufferSize);
    }
}

/*!
    \property QSensor::efficientBufferSize

    The property holds the most efficient buffer size. Normally this is 1 (which means
    no particular size is most efficient). Some sensor drivers have a FIFO buffer which
    makes it more efficient to deliver the FIFO's size worth of readings at one time.

    \sa QSensor::bufferSize, QSensor::maxBufferSize
*/

int QSensor::efficientBufferSize() const
{
    Q_D(const QSensor);
    return d->efficientBufferSize;
}

/*!
    \since 5.1
    Sets the efficient buffer size to \a efficientBufferSize. This is to be called from the
    backend.
*/
void QSensor::setEfficientBufferSize(int efficientBufferSize)
{
    Q_D(QSensor);
    if (d->efficientBufferSize != efficientBufferSize) {
        d->efficientBufferSize = efficientBufferSize;
        emit efficientBufferSizeChanged(efficientBufferSize);
    }
}

/*!
    \property QSensor::bufferSize

    This property holds the size of the buffer. By default, the buffer size is 1,
    which means no buffering.
    If the maximum buffer size is 1, then buffering is not supported
    by the sensor.

    Setting bufferSize greater than maxBufferSize will cause maxBufferSize to be used.

    Buffering is turned on when bufferSize is greater than 1. The sensor will collect
    the requested number of samples and deliver them all to the application at one time.
    They will be delivered to the application as a burst of changed readings so it is
    particularly important that the application processes each reading immediately or
    saves the values somewhere else.

    If stop() is called when buffering is on-going, the partial buffer is not delivered.

    When the sensor is started with buffering option, values are collected from that
    moment onwards. There is no pre-existing buffer that can be utilized.

    Some backends only support enabling or disabling the buffer and do not give
    control over the size. In this case, the maxBufferSize and efficientBufferSize properties
    might not be set at all, even though buffering is supported. Setting the bufferSize property
    to any value greater than 1 will enable buffering. After the sensor has been started,
    the bufferSize property will be set to the actual value by the backend.

    \sa QSensor::maxBufferSize, QSensor::efficientBufferSize
*/

int QSensor::bufferSize() const
{
    Q_D(const QSensor);
    return d->bufferSize;
}

void QSensor::setBufferSize(int bufferSize)
{
    Q_D(QSensor);
    if (d->bufferSize != bufferSize) {
        d->bufferSize = bufferSize;
        emit bufferSizeChanged(bufferSize);
    }
}

// =====================================================================

/*!
    \class QSensorFilter
    \ingroup sensors_main
    \inmodule QtSensors

    \brief The QSensorFilter class provides an efficient
           callback facility for asynchronous notifications of
           sensor changes.

    Some sensors (eg. the accelerometer) are often accessed very frequently.
    This may be slowed down by the use of signals and slots.
    The QSensorFilter interface provides a more efficient way for the
    sensor to notify your class that the sensor has changed.

    Additionally, multiple filters can be added to a sensor. They are called
    in order and each filter has the option to modify the values in the reading
    or to suppress the reading altogether.

    Note that the values in the class returned by QSensor::reading() will
    not be updated until after the filters have been run.

    \sa filter()
*/

/*!
    \internal
*/
QSensorFilter::QSensorFilter()
    : m_sensor(0)
{
}

/*!
    Notifies the attached sensor (if any) that the filter is being destroyed.
*/
QSensorFilter::~QSensorFilter()
{
    if (m_sensor)
        m_sensor->removeFilter(this);
}

/*!
    \fn QSensorFilter::filter(QSensorReading *reading)

    This function is called when the sensor \a reading changes.

    The filter can modify the reading.

    Returns true to allow the next filter to receive the value.
    If this is the last filter, returning true causes the signal
    to be emitted and the value is stored in the sensor.

    Returns false to drop the reading.
*/

/*!
    \internal
*/
void QSensorFilter::setSensor(QSensor *sensor)
{
    m_sensor = sensor;
}

// =====================================================================

/*!
    \class QSensorReading
    \ingroup sensors_main
    \inmodule QtSensors

    \brief The QSensorReading class holds the readings from the sensor.

    Note that QSensorReading is not particularly useful by itself. The interesting
    data for each sensor is defined in a sub-class of QSensorReading.
*/

/*!
    \internal
*/
QSensorReading::QSensorReading(QObject *parent, QSensorReadingPrivate *_d)
    : QObject(parent)
    , d(_d?_d:new QSensorReadingPrivate)
{
}

/*!
    \internal
*/
QSensorReading::~QSensorReading()
{
}

/*!
    \property QSensorReading::timestamp
    \brief the timestamp of the reading.

    Timestamps values are microseconds since a fixed point.
    You can use timestamps to see how far apart two sensor readings are.

    Note that sensor timestamps from different sensors may not be directly
    comparable (as they may choose different fixed points for their reference).

    \b{Note that some platforms do not deliver timestamps correctly}.
    Applications should be prepared for occasional issues that cause timestamps to jump
    backwards.
*/

/*!
    Returns the timestamp of the reading.
*/
quint64 QSensorReading::timestamp() const
{
    return d->timestamp;
}

/*!
    Sets the \a timestamp of the reading.
*/
void QSensorReading::setTimestamp(quint64 timestamp)
{
    d->timestamp = timestamp;
}

/*!
    Returns the number of extra properties that the reading has.

    Note that this does not count properties declared in QSensorReading.

    As an example, this returns 3 for QAccelerometerReading because
    there are 3 properties defined in that class.
*/
int QSensorReading::valueCount() const
{
    const QMetaObject *mo = metaObject();
    return mo->propertyCount() - mo->propertyOffset();
}

/*!
    Returns the value of the property at \a index.

    Note that this function is slower than calling the data function directly.

    Here is an example of getting a property via the different mechanisms available.

    Accessing directly provides the best performance but requires compile-time knowledge
    of the data you are accessing.

    \code
    QAccelerometerReading *reading = ...;
    qreal x = reading->x();
    \endcode

    You can also access a property by name. To do this you must call QObject::property().

    \code
    qreal x = reading->property("x").value<qreal>();
    \endcode

    Finally, you can access values via numeric index.

    \code
    qreal x = reading->value(0).value<qreal>();
    \endcode

    Note that value() can only access properties declared with Q_PROPERTY() in sub-classes
    of QSensorReading.

    \sa valueCount(), QObject::property()
*/
QVariant QSensorReading::value(int index) const
{
    // get them meta-object
    const QMetaObject *mo = metaObject();

    // determine the index of the property we want
    index += mo->propertyOffset();

    // get the meta-property
    QMetaProperty property = mo->property(index);

    // read the property
    return property.read(this);
}

/*!
    \fn QSensorReading::copyValuesFrom(QSensorReading *other)
    \internal

    Copy values from other into this reading. Implemented by sub-classes
    using the DECLARE_READING() and IMPLEMENT_READING() macros.

    Note that this method should only be called by QSensorBackend.
*/
void QSensorReading::copyValuesFrom(QSensorReading *other)
{
    QSensorReadingPrivate *my_ptr = d.data();
    QSensorReadingPrivate *other_ptr = other->d.data();
    /* Do a direct copy of the private class */
    *(my_ptr) = *(other_ptr);
}

/*!
    \fn QSensorReading::d_ptr()
    \internal
    No longer used. Exists to keep the winscw build happy.
*/

/*!
    \macro DECLARE_READING(classname)
    \relates QSensorReading
    \brief The DECLARE_READING macro adds some required methods to a reading class.

    This macro should be used for all reading classes. Pass the \a classname of your reading class.

    \code
    class MyReading : public QSensorReading
    {
        Q_OBJECT
        Q_PROPERTY(qreal myprop READ myprop)
        DECLARE_READING(MyReading)
    public:
        qreal myprop() const;
        vod setMyprop(qreal myprop);
    };
    \endcode

    \sa IMPLEMENT_READING()
*/

/*!
    \macro IMPLEMENT_READING(classname)
    \relates QSensorReading
    \brief The IMPLEMENT_READING macro implements the required methods for a reading class.

    This macro should be used for all reading classes. It should be placed into a single compilation
    unit (source file), not into a header file. Pass the \a classname of your reading class.

    \code
    IMPLEMENT_READING(MyReading)
    \endcode

    \sa DECLARE_READING()
*/

#include "moc_qsensor.cpp"
QT_END_NAMESPACE

