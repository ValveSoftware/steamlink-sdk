/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qgeocameracapabilities_p.h"

#include <QSharedData>

QT_BEGIN_NAMESPACE

class QGeoCameraCapabilitiesPrivate : public QSharedData
{
public:
    QGeoCameraCapabilitiesPrivate();
    QGeoCameraCapabilitiesPrivate(const QGeoCameraCapabilitiesPrivate &other);
    ~QGeoCameraCapabilitiesPrivate();

    QGeoCameraCapabilitiesPrivate &operator = (const QGeoCameraCapabilitiesPrivate &other);

    bool supportsBearing_;
    bool supportsRolling_;
    bool supportsTilting_;

    // this is mutable so that it can be set from accessor functions that are const
    mutable bool valid_;

    double minZoom_;
    double maxZoom_;
    double minTilt_;
    double maxTilt_;
};

QGeoCameraCapabilitiesPrivate::QGeoCameraCapabilitiesPrivate()
    : supportsBearing_(false),
      supportsRolling_(false),
      supportsTilting_(false),
      valid_(false),
      minZoom_(0.0),
      maxZoom_(0.0),
      minTilt_(0.0),
      maxTilt_(0.0) {}


QGeoCameraCapabilitiesPrivate::QGeoCameraCapabilitiesPrivate(const QGeoCameraCapabilitiesPrivate &other)
    : QSharedData(other),
      supportsBearing_(other.supportsBearing_),
      supportsRolling_(other.supportsRolling_),
      supportsTilting_(other.supportsTilting_),
      valid_(other.valid_),
      minZoom_(other.minZoom_),
      maxZoom_(other.maxZoom_),
      minTilt_(other.minTilt_),
      maxTilt_(other.maxTilt_) {}

QGeoCameraCapabilitiesPrivate::~QGeoCameraCapabilitiesPrivate() {}

QGeoCameraCapabilitiesPrivate &QGeoCameraCapabilitiesPrivate::operator = (const QGeoCameraCapabilitiesPrivate &other)
{
    if (this == &other)
        return *this;

    supportsBearing_ = other.supportsBearing_;
    supportsRolling_ = other.supportsRolling_;
    supportsTilting_ = other.supportsTilting_;
    valid_ = other.valid_;
    minZoom_ = other.minZoom_;
    maxZoom_ = other.maxZoom_;
    minTilt_ = other.minTilt_;
    maxTilt_ = other.maxTilt_;

    return *this;
}

/*!
    \class QGeoCameraCapabilities
    \inmodule QtLocation
    \ingroup QtLocation-impl
    \since 5.6
    \internal

    \brief The QGeoCameraCapabilities class describes the limitations on camera settings imposed by a mapping plugin.

    Different mapping plugins will support different ranges of zoom levels, and not all mapping plugins will
    be able to support, bearing, tilting and rolling of the camera.

    This class describes what the plugin supports, and is used to restrict changes to the camera information
    associated with a \l QGeoMap such that the camera information stays within these limits.
*/

/*!
    Constructs a camera capabilities object.
*/
QGeoCameraCapabilities::QGeoCameraCapabilities()
    : d(new QGeoCameraCapabilitiesPrivate()) {}

/*!
    Constructs a camera capabilities object from the contents of \a other.
*/
QGeoCameraCapabilities::QGeoCameraCapabilities(const QGeoCameraCapabilities &other)
    : d(other.d) {}

/*!
    Destroys this camera capabilities object.
*/
QGeoCameraCapabilities::~QGeoCameraCapabilities() {}

/*!
    Assigns the contents of \a other to this camera capabilities object and
    returns a reference to this camera capabilities object.
*/
QGeoCameraCapabilities &QGeoCameraCapabilities::operator = (const QGeoCameraCapabilities &other)
{
    if (this == &other)
        return *this;

    d = other.d;
    return *this;
}

/*!
    Returns whether this instance of the class is considered "valid". To be
    valid, the instance must have had at least one capability set (to either
    true or false) using a set method, or copied from another instance
    (such as by the assignment operator).
*/
bool QGeoCameraCapabilities::isValid() const
{
    return d->valid_;
}

/*!
    Sets the minimum zoom level supported by the associated plugin to \a maximumZoomLevel.

    Larger values of the zoom level correspond to more detailed views of the
    map.
*/
void QGeoCameraCapabilities::setMinimumZoomLevel(double minimumZoomLevel)
{
    d->minZoom_ = minimumZoomLevel;
    d->valid_ = true;
}

/*!
    Returns the minimum zoom level supported by the associated plugin.

    Larger values of the zoom level correspond to more detailed views of the
    map.
*/
double QGeoCameraCapabilities::minimumZoomLevel() const
{
    return d->minZoom_;
}

/*!
    Sets the maximum zoom level supported by the associated plugin to \a maximumZoomLevel.

    Larger values of the zoom level correspond to more detailed views of the
    map.
*/
void QGeoCameraCapabilities::setMaximumZoomLevel(double maximumZoomLevel)
{
    d->maxZoom_ = maximumZoomLevel;
    d->valid_ = true;
}

/*!
    Returns the maximum zoom level supported by the associated plugin.

    Larger values of the zoom level correspond to more detailed views of the
    map.
*/
double QGeoCameraCapabilities::maximumZoomLevel() const
{
    return d->maxZoom_;
}

/*!
    Sets whether the associated plugin can render a map when the camera
    has an arbitrary bearing to \a supportsBearing.
*/
void QGeoCameraCapabilities::setSupportsBearing(bool supportsBearing)
{
    d->supportsBearing_ = supportsBearing;
    d->valid_ = true;
}

/*!
    Returns whether the associated plugin can render a map when the camera
    has an arbitrary bearing.
*/
bool QGeoCameraCapabilities::supportsBearing() const
{
    return d->supportsBearing_;
}

/*!
    Sets whether the associated plugin can render a map when the
    camera is rolled to \a supportsRolling.
*/
void QGeoCameraCapabilities::setSupportsRolling(bool supportsRolling)
{
    d->supportsRolling_ = supportsRolling;
    d->valid_ = true;
}

/*!
    Returns whether the associated plugin can render a map when the
    camera is rolled.
*/
bool QGeoCameraCapabilities::supportsRolling() const
{
    return d->supportsRolling_;
}

/*!
    Sets whether the associated plugin can render a map when the
    camera is tilted to \a supportsTilting.
*/
void QGeoCameraCapabilities::setSupportsTilting(bool supportsTilting)
{
    d->supportsTilting_ = supportsTilting;
    d->valid_ = true;
}

/*!
    Returns whether the associated plugin can render a map when the
    camera is tilted.
*/
bool QGeoCameraCapabilities::supportsTilting() const
{
    return d->supportsTilting_;
}

/*!
    Sets the minimum tilt supported by the associated plugin to \a minimumTilt.

    The value is in degrees where 0 is equivalent to 90 degrees between
    the line of view and earth's surface, that is, looking straight down to earth.
*/
void QGeoCameraCapabilities::setMinimumTilt(double minimumTilt)
{
    d->minTilt_ = minimumTilt;
    d->valid_ = true;
}

/*!
    Returns the minimum tilt supported by the associated plugin.

    The value is in degrees where 0 is equivalent to 90 degrees between
    the line of view and earth's surface, that is, looking straight down to earth.
*/
double QGeoCameraCapabilities::minimumTilt() const
{
    return d->minTilt_;
}

/*!
    Sets the maximum tilt supported by the associated plugin to \a maximumTilt.

    The value is in degrees where 0 is equivalent to 90 degrees between
    the line of view and earth's surface, that is, looking straight down to earth.
*/
void QGeoCameraCapabilities::setMaximumTilt(double maximumTilt)
{
    d->maxTilt_ = maximumTilt;
    d->valid_ = true;
}

/*!
    Returns the maximum tilt supported by the associated plugin.

    The value is in degrees where 0 is equivalent to 90 degrees between
    the line of view and earth's surface, that is, looking straight down to earth.
*/
double QGeoCameraCapabilities::maximumTilt() const
{
    return d->maxTilt_;
}

QT_END_NAMESPACE
