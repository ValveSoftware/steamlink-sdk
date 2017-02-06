/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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


#include <QtPositioning/private/qdeclarativegeoaddress_p.h>
#include <QtPositioning/private/qdeclarativegeolocation_p.h>
#include <QtPositioning/private/qgeoprojection_p.h>

#include "qdeclarativepositionsource_p.h"
#include "qdeclarativeposition_p.h"

#include "qquickgeocoordinateanimation_p.h"
#include "locationsingleton.h"

#include <QtCore/QVariantAnimation>

#include <QtQml/qqmlextensionplugin.h>
#include <QtQml/private/qqmlvaluetype_p.h>

#include <QtPositioning/QGeoRectangle>
#include <QtPositioning/QGeoCircle>
#include <QtPositioning/QGeoLocation>

#include <QtCore/QDebug>

static void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtPositioning);
#endif
}

QT_BEGIN_NAMESPACE

/*!
    \qmlbasictype coordinate
    \inqmlmodule QtPositioning
    \ingroup qml-QtPositioning5-basictypes
    \since 5.2

    \brief The coordinate type represents and stores a geographic position.

    This type is a QML representation of \l QGeoCoordinate and represents a geographic
    position in the form of \l {latitude}, \l longitude and \l altitude attributes.
    The \l latitude attribute specifies the number of
    decimal degrees above and below the equator.  A positive latitude indicates the Northern
    Hemisphere and a negative latitude indicates the Southern Hemisphere.  The \l longitude
    attribute specifies the number of decimal degrees east and west.  A positive longitude
    indicates the Eastern Hemisphere and a negative longitude indicates the Western Hemisphere.
    The \l altitude attribute specifies the number of meters above sea level.  Together, these
    attributes specify a 3-dimensional position anywhere on or near the Earth's surface.

    The \l isValid attribute can be used to test if a coordinate is valid.  A coordinate is
    considered valid if it has a valid latitude and longitude.  A valid altitude is not required.
    The latitude must be between -90 and 90 inclusive and the longitude must be between -180 and
    180 inclusive.

    The \c coordinate type is used by many other types in the Qt Location module, for specifying
    the position of an object on a Map, the current position of a device and many other tasks.
    They also feature a number of important utility methods that make otherwise complex
    calculations simple to use, such as \l {atDistanceAndAzimuth}().

    \section1 Accuracy

    The latitude, longitude and altitude attributes stored in the coordinate type are represented
    as doubles, giving them approximately 16 decimal digits of precision -- enough to specify
    micrometers.  The calculations performed in coordinate's methods such as \l {azimuthTo}() and
    \l {distanceTo}() also use doubles for all intermediate values, but the inherent inaccuracies in
    their spherical Earth model dominate the amount of error in their output.

    \section1 Example Usage

    Use properties of type \l variant to store a \c {coordinate}.  To create a \c coordinate use
    one of the methods described below.  In all cases, specifying the \l altitude attribute is
    optional.

    To create a \c coordinate value, use the \l{QtPositioning::coordinate}{QtPositioning.coordinate()}
    function:

    \qml
    import QtPositioning 5.2

    Location { coordinate: QtPositioning.coordinate(-27.5, 153.1) }
    \endqml

    or as separate \l latitude, \l longitude and \l altitude components:

    \qml
    Location {
        coordinate {
            latitude: -27.5
            longitude: 153.1
        }
    }
    \endqml

    When integrating with C++, note that any QGeoCoordinate value passed into QML from C++ is
    automatically converted into a \c coordinate value, and vice-versa.

    \section1 Properties

    \section2 latitude

    \code
    real latitude
    \endcode

    This property holds the latitude value of the geographical position
    (decimal degrees). A positive latitude indicates the Northern Hemisphere,
    and a negative latitude indicates the Southern Hemisphere.
    If the property has not been set, its default value is NaN.

    For more details see the \l {QGeoCoordinate::latitude} property

    \section2 longitude

    \code
    real longitude
    \endcode

    This property holds the longitude value of the geographical position
    (decimal degrees). A positive longitude indicates the Eastern Hemisphere,
    and a negative longitude indicates the Western Hemisphere
    If the property has not been set, its default value is NaN.

    For more details see the \l {QGeoCoordinate::longitude} property

    \section2 altitude

    \code
    real altitude
    \endcode

    This property holds the altitude value (meters above sea level).
    If the property has not been set, its default value is NaN.

    For more details see the \l {QGeoCoordinate::altitude} property

    \section2 isValid

    \code
    bool isValid
    \endcode

    This property holds the current validity of the coordinate. Coordinates
    are considered valid if they have been set with a valid latitude and
    longitude (altitude is not required).

    The latitude must be between -90 to 90 inclusive to be considered valid,
    and the longitude must be between -180 to 180 inclusive to be considered
    valid.

    This is a read-only property.

    \section1 Methods

    \section2 distanceTo()

    \code
    real distanceTo(coordinate other)
    \endcode

    Returns the distance (in meters) from this coordinate to the coordinate specified by \a other.
    Altitude is not used in the calculation.

    This calculation returns the great-circle distance between the two coordinates, with an
    assumption that the Earth is spherical for the purpose of this calculation.

    \section2 azimuthTo()

    \code
    real azimuth(coordinate other)
    \endcode

    Returns the azimuth (or bearing) in degrees from this coordinate to the coordinate specified by
    \a other.  Altitude is not used in the calculation.

    There is an assumption that the Earth is spherical for the purpose of this calculation.

    \section2 atDistanceAndAzimuth()

    \code
    coordinate atDistanceAndAzimuth(real distance, real azimuth)
    \endcode

    Returns the coordinate that is reached by traveling \a distance metres from this coordinate at
    \a azimuth degrees along a great-circle.

    There is an assumption that the Earth is spherical for the purpose of this calculation.
*/

/*!
    \qmlbasictype geoshape
    \inqmlmodule QtPositioning
    \ingroup qml-QtPositioning5-basictypes
    \since 5.2

    \brief A geoshape type represents an abstract geographic area.

    This type is a QML representation of \l QGeoShape which is an abstract geographic area.
    It includes attributes and methods common to all geographic areas. To create objects
    that represent a valid geographic area use \l {georectangle} or \l {geocircle}.

    The \l isValid attribute can be used to test if the geoshape represents a valid geographic
    area.

    The \l isEmpty attribute can be used to test if the geoshape represents a region with a
    geometrical area of 0.

    The \l {contains}{contains()} method can be used to test if a \l {coordinate} is
    within the geoshape.

    \section1 Example Usage

    Use properties of type \l variant to store a \c {geoshape}.  To create a \c geoshape use one
    of the methods described below.

    To create a \c geoshape value, specify it as a "shape()" string:

    \qml
    import QtPositioning

    Item {
        property variant region: "shape()"
    }
    \endqml

    or with the \l {QtPositioning::shape}{QtPositioning.shape()} function:

    \qml
    import QtPositioning 5.2

    Item {
        property variant region: QtPositioning.shape()
    }
    \endqml

    When integrating with C++, note that any QGeoShape value passed into QML from C++ is
    automatically converted into a \c geoshape value, and vice-versa.

    \section1 Properties

    \section2 isEmpty

    \code
    bool isEmpty
    \endcode

    Returns whether this geoshape is empty. An empty geoshape is a region which has
    a geometrical area of 0.

    \section2 isValid

    \code
    bool isValid
    \endcode

    Returns whether this geoshape is valid.

    A geoshape is considered to be invalid if some of the data that is required to
    unambiguously describe the geoshape has not been set or has been set to an
    unsuitable value.

    \section2 type

    \code
    ShapeType type
    \endcode

    Returns the current type of the shape.

    \list
        \li GeoShape.UnknownType - The shape's type is not known.
        \li GeoShape.RectangleType - The shape is a \l georectangle.
        \li GeoShape.CircleType - The shape is a \l geocircle.
    \endlist

    This QML property was introduced by Qt 5.5.

    \section1 Methods

    \section2 contains()

    \code
    bool contains(coordinate coord)
    \endcode

    Returns true if the \l {QtPositioning::coordinate}{coordinate} specified by \a coord is within
    this geoshape; Otherwise returns false.
*/

/*!
    \qmlbasictype georectangle
    \inqmlmodule QtPositioning
    \ingroup qml-QtPositioning5-basictypes
    \since 5.2

    \brief The georectangle type represents a rectangular geographic area.

    The \c georectangle type is a \l {geoshape} that represents a
    rectangular geographic area. The type is direct representation of a \l QGeoRectangle.
    It is defined by a pair of \l {coordinate}{coordinates} which represent the top-left
    and bottom-right corners of the \c {georectangle}.  The coordinates are accessible
    from the \l topLeft and \l bottomRight attributes.

    A \c georectangle is considered invalid if the top-left or bottom-right coordinates are invalid
    or if the top-left coordinate is south of the bottom-right coordinate.

    The coordinates of the four corners of the \c georectangle can be accessed with the
    \l {topLeft}, \l {topRight}, \l {bottomLeft} and \l {bottomRight} attributes.  The \l center
    attribute can be used to get the coordinate of the center of the \c georectangle.  The \l width
    and \l height attributes can be used to get the width and height of the \c georectangle in
    degrees.  Setting one of these attributes will cause the other attributes to be adjusted
    accordingly.

    \section1 Limitations

    A \c georectangle can never cross the poles.

    If the height or center of a \c georectangle is adjusted such that it would cross one of the
    poles the height is modified such that the \c georectangle touches but does not cross the pole
    and that the center coordinate is still in the center of the \c georectangle.

    \section1 Example Usage

    Use properties of type \l variant to store a \c {georectangle}.  To create a \c georectangle
    value, use the \l {QtPositioning::rectangle}{QtPositioning.rectangle()} function:

    \qml
    import QtPositioning 5.2

    Item {
        property variant region: QtPositioning.rectangle(QtPositioning.coordinate(-27.5, 153.1), QtPositioning.coordinate(-27.6, 153.2))
    }
    \endqml

    When integrating with C++, note that any QGeoRectangle value passed into QML from C++ is
    automatically converted into a \c georectangle value, and vice-versa.

    \section1 Properties

    \section2 bottomLeft

    \code
    coordinate bottomLeft
    \endcode

    This property holds the bottom left coordinate of this georectangle.

    \section2 bottomRight

    \code
    coordinate bottomRight
    \endcode

    This property holds the bottom right coordinate of this georectangle.

    \section2 center

    \code
    coordinate center
    \endcode

    This property holds the center coordinate of this georectangle. For more details
    see \l {QGeoRectangle::setCenter()}.

    \section2 height

    \code
    double height
    \endcode

    This property holds the height of this georectangle (in degrees). For more details
    see \l {QGeoRectangle::setHeight()}.

    \note If the georectangle is invalid, it is not possible to set the height. QtPositioning
    releases prior to Qt 5.5 permitted the setting of the height even on invalid georectangles.

    \section2 topLeft

    \code
    coordinate topLeft
    \endcode

    This property holds the top left coordinate of this georectangle.

    \section2 topRight

    \code
    coordinate topRight
    \endcode

    This property holds the top right coordinate of this georectangle.

    \section2 width

    \code
    double width
    \endcode

    This property holds the width of this georectangle (in degrees). For more details
    see \l {QGeoRectangle::setWidth()}.

    \note If the georectangle is invalid, it is not possible to set the width. QtPositioning
    releases prior to Qt 5.5 permitted the setting of the width even on invalid georectangles.
*/

/*!
    \qmlbasictype geocircle
    \inqmlmodule QtPositioning
    \ingroup qml-QtPositioning5-basictypes
    \since 5.2

    \brief The geocircle type represents a circular geographic area.

    The \c geocircle type is a \l {geoshape} that represents a circular
    geographic area. It is a direct representation of a \l QGeoCircle and is defined
    in terms of a \l {coordinate} which specifies the \l center of the circle and
    a qreal which specifies the \l radius of the circle in meters.

    The circle is considered invalid if the \l center coordinate is invalid or if
    the \l radius is less than zero.

    \section1 Example Usage

    Use properties of type \l variant to store a \c {geocircle}.  To create a \c geocircle value,
    use the \l {QtPositioning::circle}{QtPositioning.circle()} function:

    \qml
    import QtPositioning 5.2

    Item {
        property variant region: QtPositioning.circle(QtPositioning.coordinate(-27.5, 153.1), 1000)
    }
    \endqml

    When integrating with C++, note that any QGeoCircle value passed into QML from C++ is
    automatically converted into a \c geocircle value, and vise-versa.

    \section1 Properties

    \section2 center

    \code
    coordinate radius
    \endcode

    This property holds the coordinate of the center of the geocircle.

    \section2 radius

    \code
    real radius
    \endcode

    This property holds the radius of the geocircle in meters.

    The default value for the radius is -1 indicating an invalid geocircle area.
*/

static QObject *singleton_type_factory(QQmlEngine *engine, QJSEngine *jsEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(jsEngine)

    return new LocationSingleton;
}

class QtPositioningDeclarativeModule: public QQmlExtensionPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid
                      FILE "plugin.json")

public:
    QtPositioningDeclarativeModule(QObject *parent = 0) : QQmlExtensionPlugin(parent) { initResources(); }
    virtual void registerTypes(const char *uri)
    {
        if (QLatin1String(uri) == QStringLiteral("QtPositioning")) {

            // @uri QtPositioning 5.0

            int major = 5;
            int minor = 0;

            qRegisterMetaType<QGeoCoordinate>();
            QMetaType::registerEqualsComparator<QGeoCoordinate>();
            qRegisterMetaType<QGeoAddress>();
            qRegisterMetaType<QGeoRectangle>();
            QMetaType::registerEqualsComparator<QGeoRectangle>();
            qRegisterMetaType<QGeoCircle>();
            QMetaType::registerEqualsComparator<QGeoCircle>();
            qRegisterMetaType<QGeoLocation>();
            qRegisterMetaType<QGeoShape>();
            QMetaType::registerEqualsComparator<QGeoShape>();

            qRegisterAnimationInterpolator<QGeoCoordinate>(q_coordinateInterpolator);

            // Register the 5.0 types
            // 5.0 is silent and not advertised
            qmlRegisterSingletonType<LocationSingleton  >(uri, major, minor, "QtPositioning", singleton_type_factory);
            qmlRegisterValueTypeEnums<QGeoShape         >(uri, major, minor, "GeoShape");
            qmlRegisterType<QDeclarativePosition        >(uri, major, minor, "Position");
            qmlRegisterType<QDeclarativePositionSource  >(uri, major, minor, "PositionSource");
            qmlRegisterType<QDeclarativeGeoAddress      >(uri, major, minor, "Address");
            qmlRegisterType<QDeclarativeGeoLocation     >(uri, major, minor, "Location");

            // Register the 5.3 types
            // Introduction of 5.3 version; existing 5.0 exports become automatically available under 5.3
            minor = 3;
            qmlRegisterType<QQuickGeoCoordinateAnimation  >(uri, major, minor, "CoordinateAnimation");
            qmlRegisterType<QDeclarativePosition, 1             >(uri, major, minor, "Position");

            minor = 4;
            qmlRegisterType<QDeclarativePosition, 2>(uri, major, minor, "Position");

            // Register the 5.8 types
            // Introduction of 5.8 version; existing 5.4 exports become automatically available under 5.8
            minor = 8;
            qmlRegisterType<QDeclarativePosition, 2>(uri, major, minor, "Position");
        } else {
            qDebug() << "Unsupported URI given to load positioning QML plugin: " << QLatin1String(uri);
        }
    }
};

#include "positioning.moc"

QT_END_NAMESPACE
