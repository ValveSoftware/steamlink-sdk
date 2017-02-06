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

#include "qgeoroutingmanagerengine.h"
#include "qgeoroutingmanagerengine_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QGeoRoutingManagerEngine
    \inmodule QtLocation
    \ingroup QtLocation-impl
    \since 5.6

    \brief The QGeoRoutingManagerEngine class provides an interface and
    convenience methods to implementers of QGeoServiceProvider plugins who want
    to provide access to geographic routing information.

    Subclasses of QGeoRoutingManagerEngine need to provide an implementation of
    calculateRoute().

    In the default implementation, supportsRouteUpdates() returns false and
    updateRoute() returns a QGeoRouteReply object containing a
    QGeoRouteReply::UnsupportedOptionError.

    If the routing service supports updating routes as they are being
    traveled, the subclass should provide an implementation of updateRoute()
    and call setSupportsRouteUpdates(true) at some point in time before
    updateRoute() is called.

    The function setSupportsRouteUpdates() is one of several functions which
    configure the reported capabilities of the engine.  If the capabilities
    of an engine differ from the default values these functions should be
    used so that the reported capabilities are accurate.

    It is important that this is done before calculateRoute(), updateRoute()
    or any of the capability reporting functions are used to prevent
    incorrect or inconsistent behavior.

    A subclass of QGeoRouteManagerEngine will often make use of a subclass
    fo QGeoRouteReply internally, in order to add any engine-specific
    data (such as a QNetworkReply object for network-based services) to the
    QGeoRouteReply instances used by the engine.

    \sa QGeoRoutingManager
*/

/*!
    Constructs a new engine with the specified \a parent, using \a parameters
    to pass any implementation specific data to the engine.
*/
QGeoRoutingManagerEngine::QGeoRoutingManagerEngine(const QVariantMap &parameters, QObject *parent)
    : QObject(parent),
      d_ptr(new QGeoRoutingManagerEnginePrivate())
{
    Q_UNUSED(parameters)
}

/*!
    Destroys this engine.
*/
QGeoRoutingManagerEngine::~QGeoRoutingManagerEngine()
{
    delete d_ptr;
}

/*!
    Sets the name which this engine implementation uses to distinguish itself
    from the implementations provided by other plugins to \a managerName.

    The combination of managerName() and managerVersion() should be unique
    amongst plugin implementations.
*/
void QGeoRoutingManagerEngine::setManagerName(const QString &managerName)
{
    d_ptr->managerName = managerName;
}

/*!
    Returns the name which this engine implementation uses to distinguish
    itself from the implementations provided by other plugins.

    The combination of managerName() and managerVersion() should be unique
    amongst plugin implementations.
*/
QString QGeoRoutingManagerEngine::managerName() const
{
    return d_ptr->managerName;
}

/*!
    Sets the version of this engine implementation to \a managerVersion.

    The combination of managerName() and managerVersion() should be unique
    amongst plugin implementations.
*/
void QGeoRoutingManagerEngine::setManagerVersion(int managerVersion)
{
    d_ptr->managerVersion = managerVersion;
}

/*!
    Returns the version of this engine implementation.

    The combination of managerName() and managerVersion() should be unique
    amongst plugin implementations.
*/
int QGeoRoutingManagerEngine::managerVersion() const
{
    return d_ptr->managerVersion;
}

/*!
\fn QGeoRouteReply *QGeoRoutingManagerEngine::calculateRoute(const QGeoRouteRequest &request)

    Begins the calculation of the route specified by \a request.

    A QGeoRouteReply object will be returned, which can be used to manage the
    routing operation and to return the results of the operation.

    This engine and the returned QGeoRouteReply object will emit signals
    indicating if the operation completes or if errors occur.

    Once the operation has completed, QGeoRouteReply::routes can be used to
    retrieve the calculated route or routes.

    If \a request includes features which are not supported by this engine, as
    reported by the methods in this engine, then a
    QGeoRouteReply::UnsupportedOptionError will occur.

    The user is responsible for deleting the returned reply object, although
    this can be done in the slot connected to QGeoRoutingManagerEngine::finished(),
    QGeoRoutingManagerEngine::error(), QGeoRouteReply::finished() or
    QGeoRouteReply::error() with deleteLater().
*/

/*!
    Begins the process of updating \a route based on the current position \a
    position.

    A QGeoRouteReply object will be returned, which can be used to manage the
    routing operation and to return the results of the operation.

    This engine and the returned QGeoRouteReply object will emit signals
    indicating if the operation completes or if errors occur.

    If supportsRouteUpdates() returns false an
    QGeoRouteReply::UnsupportedOptionError will occur.

    Once the operation has completed, QGeoRouteReply::routes can be used to
    retrieve the updated route.

    The returned route could be entirely different to the original route,
    especially if \a position is far enough away from the initial route.
    Otherwise the route will be similar, although the remaining time and
    distance will be updated and any segments of the original route which
    have been traversed will be removed.

    The user is responsible for deleting the returned reply object, although
    this can be done in the slot connected to QGeoRoutingManagerEngine::finished(),
    QGeoRoutingManagerEngine::error(), QGeoRouteReply::finished() or
    QGeoRouteReply::error() with deleteLater().
*/
QGeoRouteReply *QGeoRoutingManagerEngine::updateRoute(const QGeoRoute &route, const QGeoCoordinate &position)
{
    Q_UNUSED(route)
    Q_UNUSED(position)
    return new QGeoRouteReply(QGeoRouteReply::UnsupportedOptionError,
                              QLatin1String("The updating of routes is not supported by this service provider."), this);
}

/*!
    Sets the travel modes supported by this engine to \a travelModes.

    It is important that subclasses use this method to ensure that the engine
    reports its capabilities correctly.  If this function is not used the
    engine will report that it supports no travel modes at all.
*/
void QGeoRoutingManagerEngine::setSupportedTravelModes(QGeoRouteRequest::TravelModes travelModes)
{
    d_ptr->supportedTravelModes = travelModes;
}

/*!
    Returns the travel modes supported by this engine.
*/
QGeoRouteRequest::TravelModes QGeoRoutingManagerEngine::supportedTravelModes() const
{
    return d_ptr->supportedTravelModes;
}

/*!
    Sets the types of features that this engine can take into account
    during route planning to \a featureTypes.

    It is important that subclasses use this method to ensure that the engine
    reports its capabilities correctly.  If this function is not used the
    engine will report that it supports no feature types at all.
*/
void QGeoRoutingManagerEngine::setSupportedFeatureTypes(QGeoRouteRequest::FeatureTypes featureTypes)
{
    d_ptr->supportedFeatureTypes = featureTypes;
}

/*!
    Returns the types of features that this engine can take into account
    during route planning.
*/
QGeoRouteRequest::FeatureTypes QGeoRoutingManagerEngine::supportedFeatureTypes() const
{
    return d_ptr->supportedFeatureTypes;
}

/*!
    Sets the weightings which this engine can apply to different features
    during route planning to \a featureWeights.

    It is important that subclasses use this method to ensure that the engine
    reports its capabilities correctly.  If this function is not used the
    engine will report that it supports no feature weights at all.
*/
void QGeoRoutingManagerEngine::setSupportedFeatureWeights(QGeoRouteRequest::FeatureWeights featureWeights)
{
    d_ptr->supportedFeatureWeights = featureWeights;
    d_ptr->supportedFeatureWeights |= QGeoRouteRequest::NeutralFeatureWeight;
}

/*!
    Returns the weightings which this engine can apply to different features
    during route planning.
*/
QGeoRouteRequest::FeatureWeights QGeoRoutingManagerEngine::supportedFeatureWeights() const
{
    return d_ptr->supportedFeatureWeights;
}

/*!
    Sets the route optimizations supported by this engine to \a optimizations.

    It is important that subclasses use this method to ensure that the engine
    reports its capabilities correctly.  If this function is not used the
    engine will report that it supports no route optimizations at all.
*/
void QGeoRoutingManagerEngine::setSupportedRouteOptimizations(QGeoRouteRequest::RouteOptimizations optimizations)
{
    d_ptr->supportedRouteOptimizations = optimizations;
}

/*!
    Returns the route optimizations supported by this engine.
*/
QGeoRouteRequest::RouteOptimizations QGeoRoutingManagerEngine::supportedRouteOptimizations() const
{
    return d_ptr->supportedRouteOptimizations;
}

/*!
    Sets the levels of detail for routing segments which can be
    requested by this engine to \a segmentDetails.

    It is important that subclasses use this method to ensure that the engine
    reports its capabilities correctly.  If this function is not used the
    engine will report that it supports no segment detail at all.
*/
void QGeoRoutingManagerEngine::setSupportedSegmentDetails(QGeoRouteRequest::SegmentDetails segmentDetails)
{
    d_ptr->supportedSegmentDetails = segmentDetails;
}

/*!
    Returns the levels of detail for routing segments which can be
    requested by this engine.
*/
QGeoRouteRequest::SegmentDetails QGeoRoutingManagerEngine::supportedSegmentDetails() const
{
    return d_ptr->supportedSegmentDetails;
}

/*!
    Sets the levels of detail for navigation maneuvers which can be
    requested by this engine to \a maneuverDetails.

    It is important that subclasses use this method to ensure that the engine
    reports its capabilities correctly.  If this function is not used the
    engine will report that it supports no maneuver details at all.
*/
void QGeoRoutingManagerEngine::setSupportedManeuverDetails(QGeoRouteRequest::ManeuverDetails maneuverDetails)
{
    d_ptr->supportedManeuverDetails = maneuverDetails;
}

/*!
    Returns the levels of detail for navigation maneuvers which can be
    requested by this engine.
*/
QGeoRouteRequest::ManeuverDetails QGeoRoutingManagerEngine::supportedManeuverDetails() const
{
    return d_ptr->supportedManeuverDetails;
}

/*!
    Sets the locale to be used by this manager to \a locale.

    If this routing manager supports returning addresses and instructions
    in different languages, they will be returned in the language of \a locale.

    The locale used defaults to the system locale if this is not set.
*/
void QGeoRoutingManagerEngine::setLocale(const QLocale &locale)
{
    d_ptr->locale = locale;
    d_ptr->measurementSystem = locale.measurementSystem();
}

/*!
    Returns the locale used to hint to this routing manager about what
    language to use for addresses and instructions.
*/
QLocale QGeoRoutingManagerEngine::locale() const
{
    return d_ptr->locale;
}

/*!
    Sets the measurement system used by this manager to \a system.

    The measurement system can be set independently of the locale. Both setLocale() and this
    function set the measurement system. The value set by the last function called will be used.

    \sa measurementSystem(), locale(), setLocale()
*/
void QGeoRoutingManagerEngine::setMeasurementSystem(QLocale::MeasurementSystem system)
{
    d_ptr->measurementSystem = system;
}

/*!
    Returns the measurement system used by this manager.

    If setMeasurementSystem() has been called then the value returned by this function may be
    different to that returned by locale().\l {QLocale::measurementSystem()}{measurementSystem()}.
    In which case the value returned by this function is what will be used by the manager.

    \sa setMeasurementSystem(), setLocale()
*/
QLocale::MeasurementSystem QGeoRoutingManagerEngine::measurementSystem() const
{
    return d_ptr->measurementSystem;
}

/*!
\fn void QGeoRoutingManagerEngine::finished(QGeoRouteReply *reply)

This signal is emitted when \a reply has finished processing.

If reply::error() equals QGeoRouteReply::NoError then the processing
finished successfully.

This signal and QGeoRouteReply::finished() will be emitted at the same time.

\note Do not delete the \a reply object in the slot connected to this signal.
Use deleteLater() instead.
*/

/*!
\fn void QGeoRoutingManagerEngine::error(QGeoRouteReply *reply, QGeoRouteReply::Error error, QString errorString)

This signal is emitted when an error has been detected in the processing of
\a reply.  The QGeoRoutingManagerEngine::finished() signal will probably follow.

The error will be described by the error code \a error.  If \a errorString is
not empty it will contain a textual description of the error.

This signal and QGeoRouteReply::error() will be emitted at the same time.

\note Do not delete the \a reply object in the slot connected to this signal.
Use deleteLater() instead.
*/

/*******************************************************************************
*******************************************************************************/

QGeoRoutingManagerEnginePrivate::QGeoRoutingManagerEnginePrivate()
:   managerVersion(-1), measurementSystem(locale.measurementSystem())
{
}

QGeoRoutingManagerEnginePrivate::~QGeoRoutingManagerEnginePrivate() {}

#include "moc_qgeoroutingmanagerengine.cpp"

QT_END_NAMESPACE
