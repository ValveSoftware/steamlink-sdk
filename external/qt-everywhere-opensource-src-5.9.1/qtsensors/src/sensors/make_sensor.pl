#!/usr/bin/perl
#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the QtSensors module of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:LGPL$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 3 as published by the Free Software
## Foundation and appearing in the file LICENSE.LGPL3 included in the
## packaging of this file. Please review the following information to
## ensure the GNU Lesser General Public License version 3 requirements
## will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 2.0 or (at your option) the GNU General
## Public license version 3 or any later version approved by the KDE Free
## Qt Foundation. The licenses are as published by the Free Software
## Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-2.0.html and
## https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################

use strict;
use warnings;

use Carp;
local $Carp::CarpLevel;# = 1;

my $sensor = get_arg();
my $sensorbase = $sensor;
$sensorbase =~ s/Sensor$//;
my $reading = $sensorbase.'Reading';
my $reading_private = $reading.'Private';
my $filter = $sensorbase.'Filter';
my $no_q_sensor = $sensor;
$no_q_sensor =~ s/^.//;
my $qmlsensor = "Qml".$no_q_sensor;
my $qmlsensorbase = $qmlsensor;
$qmlsensorbase =~ s/Sensor$//;
my $qmlreading = $qmlsensorbase."Reading";
my $no_q_reading = $no_q_sensor;
$no_q_reading =~ s/Sensor$//;
$no_q_reading = $no_q_reading."Reading";

my $filebase;
eval {
    $filebase = get_arg();
};
if ($@) {
    $filebase = lc($sensor);
}

my $qmlfilebase = $filebase;
$qmlfilebase =~ s/^.//;
$qmlfilebase = "qml".$qmlfilebase;

my $pheader = $filebase."_p.h";
my $header = $filebase.".h";
my $source = $filebase.".cpp";
my $qmlsource = "../imports/sensors/".$qmlfilebase.".cpp";
my $qmlheader = "../imports/sensors/".$qmlfilebase.".h";

my $pguard = uc($pheader);
$pguard =~ s/\./_/g;

my $guard = uc($header);
$guard =~ s/\./_/g;

my $qmlguard = "QML".uc($no_q_sensor)."_H";

if (! -e $qmlheader) {
    print "Creating $qmlheader\n";
    open OUT, ">$qmlheader" or die $!;
    print OUT '
#ifndef '.$qmlguard.'
#define '.$qmlguard.'

#include "qmlsensor.h"

QT_BEGIN_NAMESPACE

class '.$sensor.';

class '.$qmlsensor.' : public QmlSensor
{
    Q_OBJECT
public:
    explicit '.$qmlsensor.'(QObject *parent = 0);
    ~'.$qmlsensor.'();

private:
    QSensor *sensor() const override;
    QmlSensorReading *createReading() const override;

    '.$sensor.' *m_sensor;
};

class '.$qmlreading.' : public QmlSensorReading
{
    Q_OBJECT
    Q_PROPERTY(qreal prop1 READ prop1 NOTIFY prop1Changed)
public:
    explicit '.$qmlreading.'('.$sensor.' *sensor);
    ~'.$qmlreading.'();

    qreal prop1() const;

Q_SIGNALS:
    void prop1Changed();

private:
    QSensorReading *reading() const override;
    void readingUpdate() override;

    '.$sensor.' *m_sensor;
    qreal m_prop1;
};

QT_END_NAMESPACE
#endif
';
    close OUT;
}

if (! -e $qmlsource) {
    print "Creating $qmlsource\n";
    open OUT, ">$qmlsource" or die $!;
    print OUT '
#include "qml'.lc($no_q_sensor).'.h"
#include <'.$sensor.'>

/*!
    \qmltype '.$no_q_sensor.'
    \instantiates '.$qmlsensor.'
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.[INSERT VERSION HERE]
    \inherits Sensor
    \brief The '.$no_q_sensor.' element reports on fubbleness.

    The '.$no_q_sensor.' element reports on fubbleness.

    This element wraps the '.$sensor.' class. Please see the documentation for
    '.$sensor.' for details.

    \sa '.$no_q_reading.'
*/

'.$qmlsensor.'::'.$qmlsensor.'(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new '.$sensor.'(this))
{
}

'.$qmlsensor.'::~'.$qmlsensor.'()
{
}

QmlSensorReading *'.$qmlsensor.'::createReading() const
{
    return new '.$qmlreading.'(m_sensor);
}

QSensor *'.$qmlsensor.'::sensor() const
{
    return m_sensor;
}

/*!
    \qmltype '.$no_q_reading.'
    \instantiates '.$qmlreading.'
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.[INSERT VERSION HERE]
    \inherits SensorReading
    \brief The '.$no_q_reading.' element holds the most recent '.$no_q_sensor.' reading.

    The '.$no_q_reading.' element holds the most recent '.$no_q_sensor.' reading.

    This element wraps the '.$reading.' class. Please see the documentation for
    '.$reading.' for details.

    This element cannot be directly created.
*/

'.$qmlreading.'::'.$qmlreading.'('.$sensor.' *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
    , m_prop1(0)
{
}

'.$qmlreading.'::~'.$qmlreading.'()
{
}

/*!
    \qmlproperty qreal '.$no_q_reading.'::prop1
    This property holds the fubble of the device.

    Please see '.$reading.'::prop1 for information about this property.
*/

qreal '.$qmlreading.'::prop1() const
{
    return m_prop1;
}

QSensorReading *'.$qmlreading.'::reading() const
{
    return m_sensor->reading();
}

void '.$qmlreading.'::readingUpdate()
{
    qreal prop1 = m_sensor->reading()->prop1();
    if (m_prop1 != prop1) {
        m_prop1 = prop1;
        Q_EMIT prop1Changed();
    }
}
';
    close OUT;
}

if (! -e $pheader) {
    print "Creating $pheader\n";
    open OUT, ">$pheader" or die $!;
    print OUT '
#ifndef '.$pguard.'
#define '.$pguard.'

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

QT_BEGIN_NAMESPACE

class '.$reading_private.'
{
public:
    '.$reading_private.'()
        : myprop(0)
    {
    }

    /*
     * Note that this class is copied so you may need to implement
     * a copy constructor if you have complex types or pointers
     * as values.
     */

    qreal myprop;
};

QT_END_NAMESPACE

#endif
';
    close OUT;
}

if (! -e $header) {
    print "Creating $header\n";
    open OUT, ">$header" or die $!;
    print OUT '
#ifndef '.$guard.'
#define '.$guard.'

#include <QtSensors/qsensor.h>

QT_BEGIN_NAMESPACE

class '.$reading_private.';

class Q_SENSORS_EXPORT '.$reading.' : public QSensorReading
{
    Q_OBJECT
    Q_PROPERTY(qreal myprop READ myprop)
    DECLARE_READING('.$reading.')
public:
    qreal myprop() const;
    void setMyprop(qreal myprop);
};

class Q_SENSORS_EXPORT '.$filter.' : public QSensorFilter
{
public:
    virtual bool filter('.$reading.' *reading) = 0;
private:
    bool filter(QSensorReading *reading) override;
};

class Q_SENSORS_EXPORT '.$sensor.' : public QSensor
{
    Q_OBJECT
public:
    explicit '.$sensor.'(QObject *parent = 0);
    ~'.$sensor.'();
    '.$reading.' *reading() const;
    static char const * const type;

private:
    Q_DISABLE_COPY('.$sensor.')
};

QT_END_NAMESPACE

#endif
';
    close OUT;
}

if (! -e $source) {
    print "Creating $source\n";
    open OUT, ">$source" or die $!;
    print OUT '
#include <'.$header.'>
#include "'.$pheader.'"

QT_BEGIN_NAMESPACE

IMPLEMENT_READING('.$reading.')

/*!
    \class '.$reading.'
    \ingroup sensors_reading
    \inmodule QtSensors
    \since 5.[INSERT VERSION HERE]

    \brief The '.$reading.' class holds readings from the [X] sensor.

    [Fill this out]

    \section2 '.$reading.' Units

    [Fill this out]
*/

/*!
    \property '.$reading.'::myprop
    \brief [what does it hold?]

    [What are the units?]
    \sa {'.$reading.' Units}
*/

qreal '.$reading.'::myprop() const
{
    return d->myprop;
}

/*!
    Sets [what?] to \a myprop.
*/
void '.$reading.'::setMyprop(qreal myprop)
{
    d->myprop = myprop;
}

// =====================================================================

/*!
    \class '.$filter.'
    \ingroup sensors_filter
    \inmodule QtSensors
    \since 5.[INSERT VERSION HERE]

    \brief The '.$filter.' class is a convenience wrapper around QSensorFilter.

    The only difference is that the filter() method features a pointer to '.$reading.'
    instead of QSensorReading.
*/

/*!
    \fn '.$filter.'::filter('.$reading.' *reading)

    Called when \a reading changes. Returns false to prevent the reading from propagating.

    \sa QSensorFilter::filter()
*/

bool '.$filter.'::filter(QSensorReading *reading)
{
    return filter(static_cast<'.$reading.'*>(reading));
}

char const * const '.$sensor.'::type("'.$sensor.'");

/*!
    \class '.$sensor.'
    \ingroup sensors_type
    \inmodule QtSensors
    \since 5.[INSERT VERSION HERE]

    \brief The '.$sensor.' class is a convenience wrapper around QSensor.

    The only behavioural difference is that this class sets the type properly.

    This class also features a reading() function that returns a '.$reading.' instead of a QSensorReading.

    For details about how the sensor works, see \l '.$reading.'.

    \sa '.$reading.'
*/

/*!
    Construct the sensor as a child of \a parent.
*/
'.$sensor.'::'.$sensor.'(QObject *parent)
    : QSensor('.$sensor.'::type, parent)
{
}

/*!
    Destroy the sensor. Stops the sensor if it has not already been stopped.
*/
'.$sensor.'::~'.$sensor.'()
{
}

/*!
    \fn '.$sensor.'::reading() const

    Returns the reading class for this sensor.

    \sa QSensor::reading()
*/

'.$reading.' *'.$sensor.'::reading() const
{
    return static_cast<'.$reading.'*>(QSensor::reading());
}

#include "moc_'.$source.'"
QT_END_NAMESPACE
';
    close OUT;
}

exit 0;


sub get_arg
{
    if (scalar(@ARGV) == 0) {
        croak "Missing arg(s)";
    }
    return shift(@ARGV);
}

