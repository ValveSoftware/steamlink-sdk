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

#include "qgeoserviceprovider.h"
#include "qgeoserviceprovider_p.h"
#include "qgeoserviceproviderfactory.h"

#include "qgeocodingmanager.h"
#include "qgeomappingmanager_p.h"
#include "qgeoroutingmanager.h"
#include "qplacemanager.h"
#include "qgeocodingmanagerengine.h"
#include "qgeomappingmanagerengine_p.h"
#include "qgeoroutingmanagerengine.h"
#include "qplacemanagerengine.h"
#include "qplacemanagerengine_p.h"

#include <QList>
#include <QString>
#include <QVariant>

#include <QPluginLoader>
#include <QDebug>
#include <QStringList>
#include <QCoreApplication>
#include <QObject>
#include <QMetaObject>
#include <QMetaEnum>
#include <QProcess>
#include <QEventLoop>
#include <QtCore/private/qfactoryloader_p.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_LIBRARY
Q_GLOBAL_STATIC_WITH_ARGS(QFactoryLoader, loader,
        ("org.qt-project.qt.geoservice.serviceproviderfactory/5.0",
         QLatin1String("/geoservices")))
#endif

/*!
    \class QGeoServiceProvider
    \inmodule QtLocation
    \ingroup QtLocation-common
    \since 5.6

    \brief The QGeoServiceProvider class aggregates access to services which provide
    geographical information.

    The Maps and Navigation API allows people to access various kinds of
    geographical information, including functionality to perform geocoding,
    routing and the display of maps.  The QGeoServiceProvider aggregates the
    access to a set of these services that are provided by a single vendor.

    It is possible to mix and match service providers for the various domains,
    so that a geocoding manager from one service provider can be used with
    a geographic routing manager from another service provider.

    This is not recommended unless the client is able to verify that the
    data provided by the different services are compatible, as differences
    in the underlying data sets could cause serious incongruences between
    the services.

    Subclasses of QGeoServiceProvider guarantee that the different services
    that they provide are interoperable.

    At this point there are two GeoServices plugins packaged with Qt. They are
    accessible using their provider names:

    \list
        \li "mapbox" -> \l {Qt Location Mapbox Plugin}{Mapbox service}
        \li "here" -> \l {Qt Location HERE Plugin}{HERE Services}
        \li "osm" -> \l {Qt Location Open Street Map Plugin}{OpenStreetMap Services}
        \li "esri" -> \l {Qt Location Esri Plugin}{ESRI Services}
    \endlist

    Each service provider must follow a naming convention for their service specific
    parameter names/keys. They use the provider name as prefix for all their
    parameter names. For example, the \l {Qt Location HERE Plugin}{HERE} service provider
    requires the \c here.app_id parameter. When a provider is loaded only those parameters are
    passed on whose parameter names start with the provider name. This avoids the sharing
    sensitive parameters such as confidential \c token or \c app_id parameters with other
    plugins.

    Please check the GeoServices plugin specific documentation to
    obtain a complete list of the available parameter names/keys and values.
*/

/*!
    \enum QGeoServiceProvider::Error

    Describes an error related to the loading and setup of a service provider plugin.

    \value NoError
        No error has occurred.
    \value NotSupportedError
        The plugin does not support this functionality.
    \value UnknownParameterError
        The plugin did not recognize one of the parameters it was given.
    \value MissingRequiredParameterError
        The plugin did not find one of the parameters it was expecting.
    \value ConnectionError
        The plugin could not connect to its backend service or database.
*/

/*!
    \enum QGeoServiceProvider::RoutingFeature

    Describes the routing features supported by the geo service provider.

    \value NoRoutingFeatures            No routing features are supported.
    \value OnlineRoutingFeature         Online routing is supported.
    \value OfflineRoutingFeature        Offline routing is supported.
    \value LocalizedRoutingFeature      Supports returning routes with localized addresses and
                                        instructions.
    \value RouteUpdatesFeature          Updating an existing route based on the current position is
                                        supported.
    \value AlternativeRoutesFeature     Supports returning alternative routes.
    \value ExcludeAreasRoutingFeature   Supports specifying a areas which the returned route must
                                        not cross.
    \value AnyRoutingFeatures           Matches a geo service provider that provides any routing
                                        features.
*/

/*!
    \enum QGeoServiceProvider::GeocodingFeature

    Describes the geocoding features supported by the geo service provider.

    \value NoGeocodingFeatures          No geocoding features are supported.
    \value OnlineGeocodingFeature       Online geocoding is supported.
    \value OfflineGeocodingFeature      Offline geocoding is supported.
    \value ReverseGeocodingFeature      Reverse geocoding is supported.
    \value LocalizedGeocodingFeature    Supports returning geocoding results with localized
                                        addresses.
    \value AnyGeocodingFeatures         Matches a geo service provider that provides any geocoding
                                        features.
*/

/*!
    \enum QGeoServiceProvider::MappingFeature

    Describes the mapping features supported by the geo service provider.

    \value NoMappingFeatures        No mapping features are supported.
    \value OnlineMappingFeature     Online mapping is supported.
    \value OfflineMappingFeature    Offline mapping is supported.
    \value LocalizedMappingFeature  Supports returning localized map data.
    \value AnyMappingFeatures       Matches a geo service provider that provides any mapping
                                    features.
*/

/*!
    \enum QGeoServiceProvider::PlacesFeature

    Describes the places features supported by the geo service provider.

    \value NoPlacesFeatures             No places features are supported.
    \value OnlinePlacesFeature          Online places is supported.
    \value OfflinePlacesFeature         Offline places is supported.
    \value SavePlaceFeature             Saving places is supported.
    \value RemovePlaceFeature           Removing or deleting places is supported.
    \value SaveCategoryFeature          Saving categories is supported.
    \value RemoveCategoryFeature        Removing or deleting categories is supported.
    \value PlaceRecommendationsFeature  Searching for recommended places similar to another place
                                        is supported.
    \value SearchSuggestionsFeature     Search suggestions is supported.
    \value LocalizedPlacesFeature       Supports returning localized place data.
    \value NotificationsFeature         Notifications of place and category changes is supported.
    \value PlaceMatchingFeature         Supports matching places from two different geo service
                                        providers.
    \value AnyPlacesFeatures            Matches a geo service provider that provides any places
                                        features.
*/

/*!
    Returns a list of names of the available service providers, for use with
    the QGeoServiceProvider constructors.
*/
QStringList QGeoServiceProvider::availableServiceProviders()
{
    return QGeoServiceProviderPrivate::plugins().keys();
}

/*!
    Constructs a QGeoServiceProvider whose backend has the name \a
    providerName, using the provided \a parameters.

    If multiple plugins have the same \a providerName, the plugin with the
    highest reported providerVersion() will be used.

    If \a allowExperimental is true then plugins marked as experimental may be used.  By default
    experimental plugins are not considered.

    If no plugin matching \a providerName was able to be loaded then error()
    and errorString() will provide details about why this is the case.

    \note Before the list of \a parameters is passed on to the to-be-loaded
    provider plugin, the list is filtered to avoid the sharing of plugin specific
    parameters with unrelated provider plugins. Plugin specific parameter
    keys must be prefixed with the provider name (e.g. \c here.app_id).
*/
QGeoServiceProvider::QGeoServiceProvider(const QString &providerName,
                                         const QVariantMap &parameters,
                                         bool allowExperimental)
    : d_ptr(new QGeoServiceProviderPrivate())
{
    d_ptr->experimental = allowExperimental;
    d_ptr->parameterMap = parameters;
    // TODO Qt 6 Remove silent nokia rename
    if (providerName == QStringLiteral("nokia"))
        d_ptr->providerName = QStringLiteral("here");
    else
        d_ptr->providerName = providerName;
    d_ptr->loadMeta();
}

/*!
    Destroys the service provider object.
*/
QGeoServiceProvider::~QGeoServiceProvider()
{
    delete d_ptr;
}

/* Template for the routingFeatures(), geocodingFeatures() etc methods.
 * Ideally, the enumName would be a template parameter, but strings
 * are not a valid const expr. :( */
template <class Flags>
Flags QGeoServiceProviderPrivate::features(const char *enumName)
{
    const QMetaObject *mo = &QGeoServiceProvider::staticMetaObject;
    const QMetaEnum en = mo->enumerator(
                mo->indexOfEnumerator(enumName));

    /* We need the typename keyword here, or Flags::enum_type will be parsed
     * as a non-type and lead to an error */
    Flags ret = typename Flags::enum_type(0);
    if (this->metaData.contains(QStringLiteral("Features"))
            && this->metaData.value(QStringLiteral("Features")).isArray()) {
        QJsonArray features = this->metaData.value(QStringLiteral("Features")).toArray();
        foreach (const QJsonValue &v, features) {
            int val = en.keyToValue(v.toString().toLatin1().constData());
            if (v.isString() && val != -1) {
                ret |= typename Flags::enum_type(val);
            }
        }
    }

    return ret;
}

/*!
    Returns the routing features supported by the geo service provider.
*/
QGeoServiceProvider::RoutingFeatures QGeoServiceProvider::routingFeatures() const
{
    return d_ptr->features<RoutingFeatures>("RoutingFeatures");
}

/*!
    Returns the geocoding features supported by the geo service provider.
*/
QGeoServiceProvider::GeocodingFeatures QGeoServiceProvider::geocodingFeatures() const
{
    return d_ptr->features<GeocodingFeatures>("GeocodingFeatures");
}

/*!
    Returns the mapping features supported by the geo service provider.
*/
QGeoServiceProvider::MappingFeatures QGeoServiceProvider::mappingFeatures() const
{
    return d_ptr->features<MappingFeatures>("MappingFeatures");
}

/*!
    Returns the places features supported by the geo service provider.
*/
QGeoServiceProvider::PlacesFeatures QGeoServiceProvider::placesFeatures() const
{
    return d_ptr->features<PlacesFeatures>("PlacesFeatures");
}

/* Sadly, these are necessary to figure out which of the factory->createX
 * methods we need to call. Ideally it would be nice to find a way to embed
 * these into the manager() template. */
template <class Engine>
Engine *createEngine(QGeoServiceProviderPrivate *)
{
    return 0;
}
template <> QGeoCodingManagerEngine *createEngine<QGeoCodingManagerEngine>(QGeoServiceProviderPrivate *d_ptr)
{
    return d_ptr->factory->createGeocodingManagerEngine(d_ptr->cleanedParameterMap, &(d_ptr->geocodeError), &(d_ptr->geocodeErrorString));
}
template <> QGeoRoutingManagerEngine *createEngine<QGeoRoutingManagerEngine>(QGeoServiceProviderPrivate *d_ptr)
{
    return d_ptr->factory->createRoutingManagerEngine(d_ptr->cleanedParameterMap, &(d_ptr->routingError), &(d_ptr->routingErrorString));
}
template <> QGeoMappingManagerEngine *createEngine<QGeoMappingManagerEngine>(QGeoServiceProviderPrivate *d_ptr)
{
    return d_ptr->factory->createMappingManagerEngine(d_ptr->cleanedParameterMap, &(d_ptr->mappingError), &(d_ptr->mappingErrorString));
}
template <> QPlaceManagerEngine *createEngine<QPlaceManagerEngine>(QGeoServiceProviderPrivate *d_ptr)
{
    return d_ptr->factory->createPlaceManagerEngine(d_ptr->cleanedParameterMap, &(d_ptr->placeError), &(d_ptr->placeErrorString));
}

/* Template for generating the code for each of the geocodingManager(),
 * mappingManager() etc methods */
template <class Manager, class Engine>
Manager *QGeoServiceProviderPrivate::manager(QGeoServiceProvider::Error *_error,
                                             QString *_errorString, Manager **_manager)
{
    // make local references just so this method is easier to read
    QGeoServiceProvider::Error &error = *_error;
    QString &errorString = *_errorString;
    Manager *&manager = *_manager;

    if (!this->factory) {
        this->filterParameterMap();
        this->loadPlugin(this->parameterMap);
    }

    if (!this->factory || error != QGeoServiceProvider::NoError)
        return 0;

    if (!manager) {
        Engine *engine = createEngine<Engine>(this);

        if (engine) {
            engine->setManagerName(
                        this->metaData.value(QStringLiteral("Provider")).toString());
            engine->setManagerVersion(
                        int(this->metaData.value(QStringLiteral("Version")).toDouble()));
            manager = new Manager(engine);
        } else if (error == QGeoServiceProvider::NoError) {
            error = QGeoServiceProvider::NotSupportedError;
            errorString = QLatin1String("The service provider does not support the ");
            errorString.append(QLatin1String(Manager::staticMetaObject.className()));
            errorString.append(QLatin1String(" type."));
        }

        if (error != QGeoServiceProvider::NoError) {
            delete manager;
            manager = 0;
            this->error = error;
            this->errorString = errorString;
        }

        if (manager && this->localeSet)
            manager->setLocale(this->locale);
    }

    if (manager) {
        this->error = QGeoServiceProvider::NoError;
        this->errorString.clear();
    }

    return manager;
}

/*!
    Returns the QGeoCodingManager made available by the service
    provider.

    This function will return 0 if the service provider does not provide
    any geocoding services.

    This function will attempt to construct a QGeoCodingManager instance
    when it is called for the first time.  If the attempt is successful the
    QGeoCodingManager will be cached, otherwise each call of this function
    will attempt to construct a QGeoCodingManager instance until the
    construction is successful.

    The QGeoCodingManager is owned by this QGeoServiceProvider and should not
    be deleted separately. Users should assume that deleting the
    QGeoServiceProvider renders the pointer returned by this method invalid.

    After this function has been called, error() and errorString() will
    report any errors which occurred during the construction of the
    QGeoCodingManager.
*/
QGeoCodingManager *QGeoServiceProvider::geocodingManager() const
{
    return d_ptr->manager<QGeoCodingManager, QGeoCodingManagerEngine>(
               &(d_ptr->geocodeError), &(d_ptr->geocodeErrorString),
               &(d_ptr->geocodingManager));
}

/*!
    Returns the QGeoMappingManager made available by the service provider.

    This function will return 0 if the service provider does not provide
    any mapping services.

    This function will attempt to construct a QGeoMappingManager instance
    when it is called for the first time.  If the attempt is successful the
    QGeoMappingManager will be cached, otherwise each call of this function
    will attempt to construct a QGeoMappingManager instance until the
    construction is successful.

    The QGeoMappingManager is owned by this QGeoServiceProvider and should not
    be deleted separately. Users should assume that deleting the
    QGeoServiceProvider renders the pointer returned by this method invalid.

    After this function has been called, error() and errorString() will
    report any errors which occurred during the construction of the
    QGeoMappingManager.

    \internal
*/
QGeoMappingManager *QGeoServiceProvider::mappingManager() const
{
    return d_ptr->manager<QGeoMappingManager, QGeoMappingManagerEngine>(
               &(d_ptr->mappingError), &(d_ptr->mappingErrorString),
               &(d_ptr->mappingManager));
}

/*!
    Returns the QGeoRoutingManager made available by the service provider.

    This function will return 0 if the service provider does not provide
    any geographic routing services.

    This function will attempt to construct a QGeoRoutingManager instance
    when it is called for the first time.  If the attempt is successful the
    QGeoRoutingManager will be cached, otherwise each call of this function
    will attempt to construct a QGeoRoutingManager instance until the
    construction is successful.

    The QGeoRoutingManager is owned by this QGeoServiceProvider and should not
    be deleted separately. Users should assume that deleting the
    QGeoServiceProvider renders the pointer returned by this method invalid.

    After this function has been called, error() and errorString() will
    report any errors which occurred during the construction of the
    QGeoRoutingManager.
*/
QGeoRoutingManager *QGeoServiceProvider::routingManager() const
{
    return d_ptr->manager<QGeoRoutingManager, QGeoRoutingManagerEngine>(
               &(d_ptr->routingError), &(d_ptr->routingErrorString),
               &(d_ptr->routingManager));
}

/*!
    Returns the QPlaceManager made available by the service provider.

    This function will attempt to construct a QPlaceManager instance
    when it is called for the first time.  If the attempt is successful the
    QPlaceManager will be cached, otherwise each call of this function
    will attempt to construct a QPlace instance until the
    construction is successful.

    The QGeoPlaceManager is owned by this QGeoServiceProvider and should not
    be deleted separately. Users should assume that deleting the
    QGeoServiceProvider renders the pointer returned by this method invalid.

    After this function has been called, error() and errorString() will
    report any errors which occurred during the construction of the QPlaceManager.
*/
QPlaceManager *QGeoServiceProvider::placeManager() const
{
    return d_ptr->manager<QPlaceManager, QPlaceManagerEngine>(
               &(d_ptr->placeError), &(d_ptr->placeErrorString),
               &(d_ptr->placeManager));
}

/*!
    Returns an error code describing the error which occurred during the
    last operation that was performed by this class.
*/
QGeoServiceProvider::Error QGeoServiceProvider::error() const
{
    return d_ptr->error;
}

/*!
    Returns a string describing the error which occurred during the
    last operation that was performed by this class.
*/
QString QGeoServiceProvider::errorString() const
{
    return d_ptr->errorString;
}

/*!
    Sets whether experimental plugins are considered when locating the
    correct plugin library for this service provider to \a allow.

    \b {Important:} this will destroy any existing managers held by this
    service provider instance. You should be sure not to attempt to use any
    pointers that you have previously retrieved after calling this method.
*/
void QGeoServiceProvider::setAllowExperimental(bool allow)
{
    d_ptr->experimental = allow;
    d_ptr->unload();
    d_ptr->loadMeta();
}

/*!
    Sets the parameters used to construct individual manager classes for
    this service provider to \a parameters.

    Before the list of \a parameters is passed on to the to-be-loaded
    service provider, the list is filtered to avoid the sharing of provider specific
    parameters with unrelated service providers. Provider specific parameter
    keys must be prefixed with the provider name (e.g. \c here.app_id).

    \b {Important:} this will destroy any existing managers held by this
    service provider instance. You should be sure not to attempt to use any
    pointers that you have previously retrieved after calling this method.
*/
void QGeoServiceProvider::setParameters(const QVariantMap &parameters)
{
    d_ptr->parameterMap = parameters;
    d_ptr->unload();
    d_ptr->loadMeta();
}

/*!
    Sets the locale used by this service provider to \a locale. If the relevant features
    (see LocalizedMappingFeature etc), this will change the languages, units
    and other locale-specific attributes of the provider's data.
*/
void QGeoServiceProvider::setLocale(const QLocale &locale)
{
    d_ptr->locale = locale;
    d_ptr->localeSet = true;

    if (d_ptr->geocodingManager)
        d_ptr->geocodingManager->setLocale(locale);
    if (d_ptr->routingManager)
        d_ptr->routingManager->setLocale(locale);
    if (d_ptr->mappingManager)
        d_ptr->mappingManager->setLocale(locale);
    if (d_ptr->placeManager)
        d_ptr->placeManager->setLocale(locale);
}

/*******************************************************************************
*******************************************************************************/

QGeoServiceProviderPrivate::QGeoServiceProviderPrivate()
    : factory(0),
      experimental(false),
      geocodingManager(0),
      routingManager(0),
      mappingManager(0),
      placeManager(0),
      geocodeError(QGeoServiceProvider::NoError),
      routingError(QGeoServiceProvider::NoError),
      mappingError(QGeoServiceProvider::NoError),
      placeError(QGeoServiceProvider::NoError),
      error(QGeoServiceProvider::NoError),
      localeSet(false)
{
    metaData.insert(QStringLiteral("index"), -1);
}

QGeoServiceProviderPrivate::~QGeoServiceProviderPrivate()
{
    delete geocodingManager;
    delete routingManager;
    delete mappingManager;
    delete placeManager;
}

void QGeoServiceProviderPrivate::unload()
{
    delete geocodingManager;
    geocodingManager = 0;

    delete routingManager;
    routingManager = 0;

    delete mappingManager;
    mappingManager = 0;

    delete placeManager;
    placeManager = 0;

    factory = 0;
    error = QGeoServiceProvider::NoError;
    errorString = QLatin1String("");
    metaData = QJsonObject();
    metaData.insert(QStringLiteral("index"), -1);
}

/* Filter out any parameter that doesn't match any plugin */
void QGeoServiceProviderPrivate::filterParameterMap()
{
    const auto availablePlugins = QGeoServiceProviderPrivate::plugins();

    cleanedParameterMap = parameterMap;
    for (auto it = availablePlugins.keyBegin(), end = availablePlugins.keyEnd(); it != end; ++it) {
        if (*it == providerName) // don't remove parameters for current provider
            continue;

        QVariantMap::iterator i = cleanedParameterMap.begin();
        while (i != cleanedParameterMap.end()) {
            // remove every parameter meant for other plugins
            if (i.key().startsWith(QString(*it + QLatin1Char('.'))))
                i = cleanedParameterMap.erase(i);
            else
                ++i;
        }
    }
}

void QGeoServiceProviderPrivate::loadMeta()
{
    factory = 0;
    metaData = QJsonObject();
    metaData.insert(QStringLiteral("index"), -1);
    error = QGeoServiceProvider::NotSupportedError;
    errorString = QString(QLatin1String("The geoservices provider %1 is not supported.")).arg(providerName);

    QList<QJsonObject> candidates = QGeoServiceProviderPrivate::plugins().values(providerName);

    int versionFound = -1;
    int idx = -1;

    // figure out which version of the plugin we want
    // (always latest unless experimental)
    for (int i = 0; i < candidates.size(); ++i) {
        QJsonObject meta = candidates[i];
        if (meta.contains(QStringLiteral("Version"))
                && meta.value(QStringLiteral("Version")).isDouble()
                && meta.contains(QStringLiteral("Experimental"))
                && meta.value(QStringLiteral("Experimental")).isBool()) {
            int ver = int(meta.value(QStringLiteral("Version")).toDouble());
            if (ver > versionFound && !(!experimental && meta.value(QStringLiteral("Experimental")).toBool())) {
                versionFound = ver;
                idx = i;
            }
        }
    }

    if (idx != -1) {
        error = QGeoServiceProvider::NoError;
        errorString = QStringLiteral("");
        metaData = candidates[idx];
    }
}

void QGeoServiceProviderPrivate::loadPlugin(const QVariantMap &parameters)
{
    Q_UNUSED(parameters)

    if (int(metaData.value(QStringLiteral("index")).toDouble()) < 0) {
        error = QGeoServiceProvider::NotSupportedError;
        errorString = QString(QLatin1String("The geoservices provider is not supported."));
        factory = 0;
        return;
    }

    error = QGeoServiceProvider::NoError;
    errorString = QLatin1String("");

    int idx = int(metaData.value(QStringLiteral("index")).toDouble());

    // load the actual plugin
    factory = qobject_cast<QGeoServiceProviderFactory *>(loader()->instance(idx));
}

QHash<QString, QJsonObject> QGeoServiceProviderPrivate::plugins(bool reload)
{
    static QHash<QString, QJsonObject> plugins;
    static bool alreadyDiscovered = false;

    if (reload == true)
        alreadyDiscovered = false;

    if (!alreadyDiscovered) {
        loadPluginMetadata(plugins);
        alreadyDiscovered = true;
    }
    return plugins;
}

void QGeoServiceProviderPrivate::loadPluginMetadata(QHash<QString, QJsonObject> &list)
{
    QFactoryLoader *l = loader();
    QList<QJsonObject> meta = l->metaData();
    for (int i = 0; i < meta.size(); ++i) {
        QJsonObject obj = meta.at(i).value(QStringLiteral("MetaData")).toObject();
        obj.insert(QStringLiteral("index"), i);
        list.insertMulti(obj.value(QStringLiteral("Provider")).toString(), obj);
    }
}


QT_END_NAMESPACE

