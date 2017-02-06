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

#include "qdeclarativegeoserviceprovider_p.h"

#include <QtQml/QQmlInfo>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Plugin
    \instantiates QDeclarativeGeoServiceProvider
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-common
    \since Qt Location 5.5

    \brief The Plugin type describes a Location based services plugin.

    The Plugin type is used to declaratively specify which available
    GeoServices plugin should be used for various tasks in the Location API.
    Plugins are used by \l Map, \l RouteModel, and \l GeocodeModel
    types, as well as a variety of others.

    Plugins recognized by the system have a \l name property, a simple string
    normally indicating the name of the service that the Plugin retrieves
    data from. They also have a variety of features, which can be test for using the
    \l {supportsRouting()}, \l {supportsGeocoding()}, \l {supportsMapping()} and
    \l {supportsPlaces()} methods.

    When a Plugin object is created, it is "detached" and not associated with
    any actual service plugin. Once it has received information via setting
    its \l name, \l preferred, or \l required properties, it will choose an
    appropriate service plugin to attach to. Plugin objects can only be
    attached once; to use multiple plugins, create multiple Plugin objects.

    \section2 Example Usage

    The following snippet shows a Plugin object being created with the
    \l required and \l preferred properties set. This Plugin will attach to the
    first found plugin that supports both mapping and geocoding, and will
    prefer plugins named "here" or "osm" to any others.

    \code
    Plugin {
        id: plugin
        preferred: ["here", "osm"]
        required: Plugin.AnyMappingFeatures | Plugin.AnyGeocodingFeatures
    }
    \endcode
*/

QDeclarativeGeoServiceProvider::QDeclarativeGeoServiceProvider(QObject *parent)
:   QObject(parent),
    sharedProvider_(0),
    required_(new QDeclarativeGeoServiceProviderRequirements),
    complete_(false),
    experimental_(false)
{
    locales_.append(QLocale().name());
}

QDeclarativeGeoServiceProvider::~QDeclarativeGeoServiceProvider()
{
    delete required_;
    delete sharedProvider_;
}



/*!
    \qmlproperty string Plugin::name

    This property holds the name of the plugin. Setting this property
    will cause the Plugin to only attach to a plugin with exactly this
    name. The value of \l required will be ignored.
*/
void QDeclarativeGeoServiceProvider::setName(const QString &name)
{
    if (name_ == name)
        return;

    name_ = name;
    delete sharedProvider_;
    sharedProvider_ = new QGeoServiceProvider(name_, parameterMap());
    sharedProvider_->setLocale(locales_.at(0));
    sharedProvider_->setAllowExperimental(experimental_);

    emit nameChanged(name_);
    emit attached();
}

QString QDeclarativeGeoServiceProvider::name() const
{
    return name_;
}


/*!
    \qmlproperty stringlist Plugin::availableServiceProviders

    This property holds a list of all available service plugins' names. This
    can be used to manually enumerate the available plugins if the
    control provided by \l name and \l required is not sufficient for your
    needs.
*/
QStringList QDeclarativeGeoServiceProvider::availableServiceProviders()
{
    return QGeoServiceProvider::availableServiceProviders();
}

/*!
    \internal
*/
void QDeclarativeGeoServiceProvider::componentComplete()
{
    complete_ = true;
    if (!name_.isEmpty()) {
        return;
    }

    if (!prefer_.isEmpty()
            || required_->mappingRequirements() != NoMappingFeatures
            || required_->routingRequirements() != NoRoutingFeatures
            || required_->geocodingRequirements() != NoGeocodingFeatures
            || required_->placesRequirements() != NoPlacesFeatures) {

        QStringList providers = QGeoServiceProvider::availableServiceProviders();

        /* first check any preferred plugins */
        foreach (const QString &name, prefer_) {
            if (providers.contains(name)) {
                // so we don't try it again later
                providers.removeAll(name);

                QGeoServiceProvider sp(name, parameterMap(), experimental_);
                if (required_->matches(&sp)) {
                    setName(name);
                    return;
                }
            }
        }

        /* then try the rest */
        foreach (const QString &name, providers) {
            QGeoServiceProvider sp(name, parameterMap(), experimental_);
            if (required_->matches(&sp)) {
                setName(name);
                return;
            }
        }

        qmlInfo(this) << "Could not find a plugin with the required features to attach to";
    }
}

/*!
    \qmlmethod bool Plugin::supportsGeocoding(GeocodingFeatures features)

    This method returns a boolean indicating whether the specified set of \a features are supported
    by the geo service provider plugin.  True is returned if all specified \a features are
    supported; otherwise false is returned.

    The \a features parameter can be any flag combination of:
    \table
        \header
            \li Feature
            \li Description
        \row
            \li Plugin.NoGeocodingFeatures
            \li No geocoding features are supported.
        \row
            \li Plugin.OnlineGeocodingFeature
            \li Online geocoding is supported.
        \row
            \li Plugin.OfflineGeocodingFeature
            \li Offline geocoding is supported.
        \row
            \li Plugin.ReverseGeocodingFeature
            \li Reverse geocoding is supported.
        \row
            \li Plugin.LocalizedGeocodingFeature
            \li Supports returning geocoding results with localized addresses.
        \row
            \li Plugin.AnyGeocodingFeatures
            \li Matches a geo service provider that provides any geocoding features.
    \endtable
*/
bool QDeclarativeGeoServiceProvider::supportsGeocoding(const GeocodingFeatures &feature) const
{
    QGeoServiceProvider *sp = sharedGeoServiceProvider();
    QGeoServiceProvider::GeocodingFeatures f =
            static_cast<QGeoServiceProvider::GeocodingFeature>(int(feature));
    if (f == QGeoServiceProvider::AnyGeocodingFeatures)
        return (sp && (sp->geocodingFeatures() != QGeoServiceProvider::NoGeocodingFeatures));
    else
        return (sp && (sp->geocodingFeatures() & f) == f);
}

/*!
    \qmlmethod bool Plugin::supportsMapping(MappingFeatures features)

    This method returns a boolean indicating whether the specified set of \a features are supported
    by the geo service provider plugin.  True is returned if all specified \a features are
    supported; otherwise false is returned.

    The \a features parameter can be any flag combination of:
    \table
        \header
            \li Feature
            \li Description
        \row
            \li Plugin.NoMappingFeatures
            \li No mapping features are supported.
        \row
            \li Plugin.OnlineMappingFeature
            \li Online mapping is supported.
        \row
            \li Plugin.OfflineMappingFeature
            \li Offline mapping is supported.
        \row
            \li Plugin.LocalizedMappingFeature
            \li Supports returning localized map data.
        \row
            \li Plugin.AnyMappingFeatures
            \li Matches a geo service provider that provides any mapping features.
    \endtable
*/
bool QDeclarativeGeoServiceProvider::supportsMapping(const MappingFeatures &feature) const
{
    QGeoServiceProvider *sp = sharedGeoServiceProvider();
    QGeoServiceProvider::MappingFeatures f =
            static_cast<QGeoServiceProvider::MappingFeature>(int(feature));
    if (f == QGeoServiceProvider::AnyMappingFeatures)
        return (sp && (sp->mappingFeatures() != QGeoServiceProvider::NoMappingFeatures));
    else
        return (sp && (sp->mappingFeatures() & f) == f);
}

/*!
    \qmlmethod bool Plugin::supportsRouting(RoutingFeatures features)

    This method returns a boolean indicating whether the specified set of \a features are supported
    by the geo service provider plugin.  True is returned if all specified \a features are
    supported; otherwise false is returned.

    The \a features parameter can be any flag combination of:
    \table
        \header
            \li Feature
            \li Description
        \row
            \li Plugin.NoRoutingFeatures
            \li No routing features are supported.
        \row
            \li Plugin.OnlineRoutingFeature
            \li Online routing is supported.
        \row
            \li Plugin.OfflineRoutingFeature
            \li Offline routing is supported.
        \row
            \li Plugin.LocalizedRoutingFeature
            \li Supports returning routes with localized addresses and instructions.
        \row
            \li Plugin.RouteUpdatesFeature
            \li Updating an existing route based on the current position is supported.
        \row
            \li Plugin.AlternativeRoutesFeature
            \li Supports returning alternative routes.
        \row
            \li Plugin.ExcludeAreasRoutingFeature
            \li Supports specifying a areas which the returned route must not cross.
        \row
            \li Plugin.AnyRoutingFeatures
            \li Matches a geo service provider that provides any routing features.
    \endtable
*/
bool QDeclarativeGeoServiceProvider::supportsRouting(const RoutingFeatures &feature) const
{
    QGeoServiceProvider *sp = sharedGeoServiceProvider();
    QGeoServiceProvider::RoutingFeatures f =
            static_cast<QGeoServiceProvider::RoutingFeature>(int(feature));
    if (f == QGeoServiceProvider::AnyRoutingFeatures)
        return (sp && (sp->routingFeatures() != QGeoServiceProvider::NoRoutingFeatures));
    else
        return (sp && (sp->routingFeatures() & f) == f);
}

/*!
    \qmlmethod bool Plugin::supportsPlaces(PlacesFeatures features)

    This method returns a boolean indicating whether the specified set of \a features are supported
    by the geo service provider plugin.  True is returned if all specified \a features are
    supported; otherwise false is returned.

    The \a features parameter can be any flag combination of:
    \table
        \header
            \li Feature
            \li Description
        \row
            \li Plugin.NoPlacesFeatures
            \li No places features are supported.
        \row
            \li Plugin.OnlinePlacesFeature
            \li Online places is supported.
        \row
            \li Plugin.OfflinePlacesFeature
            \li Offline places is supported.
        \row
            \li Plugin.SavePlaceFeature
            \li Saving categories is supported.
        \row
            \li Plugin.RemovePlaceFeature
            \li Removing or deleting places is supported.
        \row
            \li Plugin.PlaceRecommendationsFeature
            \li Searching for recommended places similar to another place is supported.
        \row
            \li Plugin.SearchSuggestionsFeature
            \li Search suggestions is supported.
        \row
            \li Plugin.LocalizedPlacesFeature
            \li Supports returning localized place data.
        \row
            \li Plugin.NotificationsFeature
            \li Notifications of place and category changes is supported.
        \row
            \li Plugin.PlaceMatchingFeature
            \li Supports matching places from two different geo service providers.
        \row
            \li Plugin.AnyPlacesFeatures
            \li Matches a geo service provider that provides any places features.
    \endtable
*/
bool QDeclarativeGeoServiceProvider::supportsPlaces(const PlacesFeatures &features) const
{
    QGeoServiceProvider *sp = sharedGeoServiceProvider();
    QGeoServiceProvider::PlacesFeatures f =
            static_cast<QGeoServiceProvider::PlacesFeature>(int(features));
    if (f == QGeoServiceProvider::AnyPlacesFeatures)
        return (sp && (sp->placesFeatures() != QGeoServiceProvider::NoPlacesFeatures));
    else
        return (sp && (sp->placesFeatures() & f) == f);
}

/*!
    \qmlproperty enumeration Plugin::required

    This property contains the set of features that will be required by the
    Plugin object when choosing which service plugin to attach to. If the
    \l name property is set, this has no effect.

    Any of the following values or a bitwise combination of multiple values
    may be set:

    \list
    \li Plugin.NoFeatures
    \li Plugin.GeocodingFeature
    \li Plugin.ReverseGeocodingFeature
    \li Plugin.RoutingFeature
    \li Plugin.MappingFeature
    \li Plugin.AnyPlacesFeature
    \endlist
*/
QDeclarativeGeoServiceProviderRequirements *QDeclarativeGeoServiceProvider::requirements() const
{
    return required_;
}

/*!
    \qmlproperty stringlist Plugin::preferred

    This property contains an ordered list of preferred plugin names, which
    will be checked for the required features set in \l{Plugin::required}{required}
    before any other available plugins are checked.
*/
QStringList QDeclarativeGeoServiceProvider::preferred() const
{
    return prefer_;
}

void QDeclarativeGeoServiceProvider::setPreferred(const QStringList &val)
{
    prefer_ = val;
    emit preferredChanged(prefer_);
}

/*!
    \qmlproperty bool Plugin::isAttached

    This property indicates if the Plugin is attached to another Plugin.
*/
bool QDeclarativeGeoServiceProvider::isAttached() const
{
    return (sharedProvider_ != 0);
}

/*!
    \qmlproperty bool Plugin::allowExperimental

    This property indicates if experimental plugins can be used.
*/
bool QDeclarativeGeoServiceProvider::allowExperimental() const
{
    return experimental_;
}

void QDeclarativeGeoServiceProvider::setAllowExperimental(bool allow)
{
    if (experimental_ == allow)
        return;

    experimental_ = allow;
    if (sharedProvider_)
        sharedProvider_->setAllowExperimental(allow);

    emit allowExperimentalChanged(allow);
}

/*!
    \internal
*/
QGeoServiceProvider *QDeclarativeGeoServiceProvider::sharedGeoServiceProvider() const
{
    return sharedProvider_;
}

/*!
    \qmlproperty stringlist Plugin::locales

    This property contains an ordered list of preferred plugin locales.  If the first locale cannot be accommodated, then
    the backend falls back to using the second, and so on.  By default the locales property contains the system locale.

    The locales are specified as strings which have the format
    "language[_script][_country]" or "C", where:

    \list
    \li language is a lowercase, two-letter, ISO 639 language code,
    \li script is a titlecase, four-letter, ISO 15924 script code,
    \li country is an uppercase, two- or three-letter, ISO 3166 country code (also "419" as defined by United Nations),
    \li the "C" locale is identical in behavior to English/UnitedStates as per QLocale
    \endlist

    If the first specified locale cannot be accommodated, the \l {Plugin} falls back to the next and so forth.
    Some \l {Plugin} backends may not support a set of locales which are rigidly defined.  An arbitrary
    example is that some \l {Place}'s in France could have French and English localizations, while
    certain areas in America may only have the English localization available.  In the above scenario,
    the set of supported locales is context dependent on the search location.

    If the \l {Plugin} cannot accommodate any of the preferred locales, the manager falls
    back to using a supported language that is backend specific.

    For \l {Plugin}'s that do not support locales, the locales list is always empty.

    The following code demonstrates how to set a single and multiple locales:
    \snippet declarative/plugin.qml Plugin locale
*/
QStringList QDeclarativeGeoServiceProvider::locales() const
{
    return locales_;
}

void QDeclarativeGeoServiceProvider::setLocales(const QStringList &locales)
{
    if (locales_ == locales)
        return;

    locales_ = locales;

    if (locales_.isEmpty())
        locales_.append(QLocale().name());

    if (sharedProvider_)
        sharedProvider_->setLocale(locales_.at(0));

    emit localesChanged();
}

/*!
    \qmlproperty list<PluginParameter> Plugin::parameters
    \default

    This property holds the list of plugin parameters.
*/
QQmlListProperty<QDeclarativeGeoServiceProviderParameter> QDeclarativeGeoServiceProvider::parameters()
{
    return QQmlListProperty<QDeclarativeGeoServiceProviderParameter>(this,
            0,
            parameter_append,
            parameter_count,
            parameter_at,
            parameter_clear);
}

/*!
    \internal
*/
void QDeclarativeGeoServiceProvider::parameter_append(QQmlListProperty<QDeclarativeGeoServiceProviderParameter> *prop, QDeclarativeGeoServiceProviderParameter *parameter)
{
    QDeclarativeGeoServiceProvider *p = static_cast<QDeclarativeGeoServiceProvider *>(prop->object);
    p->parameters_.append(parameter);
    if (p->sharedProvider_)
        p->sharedProvider_->setParameters(p->parameterMap());
}

/*!
    \internal
*/
int QDeclarativeGeoServiceProvider::parameter_count(QQmlListProperty<QDeclarativeGeoServiceProviderParameter> *prop)
{
    return static_cast<QDeclarativeGeoServiceProvider *>(prop->object)->parameters_.count();
}

/*!
    \internal
*/
QDeclarativeGeoServiceProviderParameter *QDeclarativeGeoServiceProvider::parameter_at(QQmlListProperty<QDeclarativeGeoServiceProviderParameter> *prop, int index)
{
    return static_cast<QDeclarativeGeoServiceProvider *>(prop->object)->parameters_[index];
}

/*!
    \internal
*/
void QDeclarativeGeoServiceProvider::parameter_clear(QQmlListProperty<QDeclarativeGeoServiceProviderParameter> *prop)
{
    QDeclarativeGeoServiceProvider *p = static_cast<QDeclarativeGeoServiceProvider *>(prop->object);
    p->parameters_.clear();
    if (p->sharedProvider_)
        p->sharedProvider_->setParameters(p->parameterMap());
}

/*!
    \internal
*/
QVariantMap QDeclarativeGeoServiceProvider::parameterMap() const
{
    QVariantMap map;

    for (int i = 0; i < parameters_.size(); ++i) {
        QDeclarativeGeoServiceProviderParameter *parameter = parameters_.at(i);
        map.insert(parameter->name(), parameter->value());
    }

    return map;
}

/*******************************************************************************
*******************************************************************************/

QDeclarativeGeoServiceProviderRequirements::QDeclarativeGeoServiceProviderRequirements(QObject *parent)
    : QObject(parent),
      mapping_(QDeclarativeGeoServiceProvider::NoMappingFeatures),
      routing_(QDeclarativeGeoServiceProvider::NoRoutingFeatures),
      geocoding_(QDeclarativeGeoServiceProvider::NoGeocodingFeatures),
      places_(QDeclarativeGeoServiceProvider::NoPlacesFeatures)
{
}

QDeclarativeGeoServiceProviderRequirements::~QDeclarativeGeoServiceProviderRequirements()
{
}

/*!
    \internal
*/
QDeclarativeGeoServiceProvider::MappingFeatures QDeclarativeGeoServiceProviderRequirements::mappingRequirements() const
{
    return mapping_;
}

/*!
    \internal
*/
void QDeclarativeGeoServiceProviderRequirements::setMappingRequirements(const QDeclarativeGeoServiceProvider::MappingFeatures &features)
{
    if (mapping_ == features)
        return;

    mapping_ = features;
    emit mappingRequirementsChanged(mapping_);
    emit requirementsChanged();
}

/*!
    \internal
*/
QDeclarativeGeoServiceProvider::RoutingFeatures QDeclarativeGeoServiceProviderRequirements::routingRequirements() const
{
    return routing_;
}

/*!
    \internal
*/
void QDeclarativeGeoServiceProviderRequirements::setRoutingRequirements(const QDeclarativeGeoServiceProvider::RoutingFeatures &features)
{
    if (routing_ == features)
        return;

    routing_ = features;
    emit routingRequirementsChanged(routing_);
    emit requirementsChanged();
}

/*!
    \internal
*/
QDeclarativeGeoServiceProvider::GeocodingFeatures QDeclarativeGeoServiceProviderRequirements::geocodingRequirements() const
{
    return geocoding_;
}

/*!
    \internal
*/
void QDeclarativeGeoServiceProviderRequirements::setGeocodingRequirements(const QDeclarativeGeoServiceProvider::GeocodingFeatures &features)
{
    if (geocoding_ == features)
        return;

    geocoding_ = features;
    emit geocodingRequirementsChanged(geocoding_);
    emit requirementsChanged();
}

/*!
    \internal

    */
QDeclarativeGeoServiceProvider::PlacesFeatures QDeclarativeGeoServiceProviderRequirements::placesRequirements() const
{
    return places_;
}

/*!
    \internal
*/
void QDeclarativeGeoServiceProviderRequirements::setPlacesRequirements(const QDeclarativeGeoServiceProvider::PlacesFeatures &features)
{
    if (places_ == features)
        return;

    places_ = features;
    emit placesRequirementsChanged(places_);
    emit requirementsChanged();
}

/*!
    \internal
*/
bool QDeclarativeGeoServiceProviderRequirements::matches(const QGeoServiceProvider *provider) const
{
    QGeoServiceProvider::MappingFeatures mapping =
            static_cast<QGeoServiceProvider::MappingFeatures>(int(mapping_));

    // extra curlies here to avoid "dangling" else, which could belong to either if
    // same goes for all the rest of these blocks
    if (mapping == QGeoServiceProvider::AnyMappingFeatures) {
        if (provider->mappingFeatures() == QGeoServiceProvider::NoMappingFeatures)
            return false;
    } else {
        if ((provider->mappingFeatures() & mapping) != mapping)
            return false;
    }

    QGeoServiceProvider::RoutingFeatures routing =
            static_cast<QGeoServiceProvider::RoutingFeatures>(int(routing_));

    if (routing == QGeoServiceProvider::AnyRoutingFeatures) {
        if (provider->routingFeatures() == QGeoServiceProvider::NoRoutingFeatures)
            return false;
    } else {
        if ((provider->routingFeatures() & routing) != routing)
            return false;
    }

    QGeoServiceProvider::GeocodingFeatures geocoding =
            static_cast<QGeoServiceProvider::GeocodingFeatures>(int(geocoding_));

    if (geocoding == QGeoServiceProvider::AnyGeocodingFeatures) {
        if (provider->geocodingFeatures() == QGeoServiceProvider::NoGeocodingFeatures)
            return false;
    } else {
        if ((provider->geocodingFeatures() & geocoding) != geocoding)
            return false;
    }

    QGeoServiceProvider::PlacesFeatures places =
            static_cast<QGeoServiceProvider::PlacesFeatures>(int(places_));

    if (places == QGeoServiceProvider::AnyPlacesFeatures) {
        if (provider->placesFeatures() == QGeoServiceProvider::NoPlacesFeatures)
            return false;
    } else {
        if ((provider->placesFeatures() & places) != places)
            return false;
    }

    return true;
}

/*******************************************************************************
*******************************************************************************/

/*!
    \qmltype PluginParameter
    \instantiates QDeclarativeGeoServiceProviderParameter
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-common
    \since Qt Location 5.5

    \brief The PluginParameter type describes a parameter to a \l Plugin.

    The PluginParameter object is used to provide a parameter of some kind
    to a Plugin. Typically these parameters contain details like an application
    token for access to a service, or a proxy server to use for network access.

    To set such a parameter, declare a PluginParameter inside a \l Plugin
    object, and give it \l{name} and \l{value} properties. A list of valid
    parameter names for each plugin is available from the
    \l {Qt Location#Plugin References and Parameters}{plugin reference pages}.

    \section2 Example Usage

    The following example shows an instantiation of the \l {Qt Location HERE Plugin}{HERE} plugin
    with a mapping API \e app_id and \e token pair specific to the application.

    \code
    Plugin {
        name: "here"
        PluginParameter { name: "here.app_id"; value: "EXAMPLE_API_ID" }
        PluginParameter { name: "here.token"; value: "EXAMPLE_TOKEN_123" }
    }
    \endcode
*/

QDeclarativeGeoServiceProviderParameter::QDeclarativeGeoServiceProviderParameter(QObject *parent)
    : QObject(parent) {}

QDeclarativeGeoServiceProviderParameter::~QDeclarativeGeoServiceProviderParameter() {}

/*!
    \qmlproperty string PluginParameter::name

    This property holds the name of the plugin parameter as a single formatted string.
*/
void QDeclarativeGeoServiceProviderParameter::setName(const QString &name)
{
    if (name_ == name)
        return;

    name_ = name;

    emit nameChanged(name_);
}

QString QDeclarativeGeoServiceProviderParameter::name() const
{
    return name_;
}

/*!
    \qmlproperty QVariant PluginParameter::value

    This property holds the value of the plugin parameter which support different types of values (variant).
*/
void QDeclarativeGeoServiceProviderParameter::setValue(const QVariant &value)
{
    if (value_ == value)
        return;

    value_ = value;

    emit valueChanged(value_);
}

QVariant QDeclarativeGeoServiceProviderParameter::value() const
{
    return value_;
}

/*******************************************************************************
*******************************************************************************/

#include "moc_qdeclarativegeoserviceprovider_p.cpp"

QT_END_NAMESPACE

