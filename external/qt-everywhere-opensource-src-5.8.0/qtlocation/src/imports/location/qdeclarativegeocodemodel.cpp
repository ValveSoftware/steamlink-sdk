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

#include "qdeclarativegeocodemodel_p.h"
#include "error_messages.h"

#include <QtCore/QCoreApplication>
#include <QtQml/QQmlInfo>
#include <QtPositioning/QGeoCircle>
#include <QtLocation/QGeoServiceProvider>
#include <QtLocation/QGeoCodingManager>

QT_BEGIN_NAMESPACE

/*!
    \qmltype GeocodeModel
    \instantiates QDeclarativeGeocodeModel
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-geocoding
    \since Qt Location 5.5

    \brief The GeocodeModel type provides support for searching operations
           related to geographic information.

    The GeocodeModel type is used as part of a model/view grouping to
    match addresses or search strings with geographic locations. How the
    geographic locations generated are used or displayed is decided by any
    Views attached to the GeocodeModel (for example a \l MapItemView or \l{ListView}).

    Like \l Map and \l RouteModel, all the data for a GeocodeModel to work
    comes from a services plugin. This is contained in the \l{plugin} property,
    and this must be set before the GeocodeModel can do any useful work.

    Once the plugin is set, the \l{query} property can be used to specify the
    address or search string to match. If \l{autoUpdate} is enabled, the Model
    will update its output automatically. Otherwise, the \l{update} method may
    be used. By default, autoUpdate is disabled.

    The data stored and returned in the GeocodeModel consists of \l{Location}
    objects, as a list with the role name "locationData". See the documentation
    for \l{Location} for further details on its structure and contents.

    \section2 Example Usage

    The following snippet is two-part, showing firstly the declaration of
    objects, and secondly a short piece of procedural code using it. We set
    the geocodeModel's \l{autoUpdate} property to false, and call \l{update} once
    the query is set up. In this case, as we use a string value in \l{query},
    only one update would occur, even with autoUpdate enabled. However, if we
    provided an \l{Address} object we may inadvertently trigger multiple
    requests whilst setting its properties.

    \code
    Plugin {
        id: aPlugin
    }

    GeocodeModel {
        id: geocodeModel
        plugin: aPlugin
        autoUpdate: false
    }
    \endcode

    \code
    {
        geocodeModel.query = "53 Brandl St, Eight Mile Plains, Australia"
        geocodeModel.update()
    }
    \endcode
*/

/*!
    \qmlsignal QtLocation::GeocodeModel::locationsChanged()

    This signal is emitted when locations in the model have changed.

    \sa count
*/


QDeclarativeGeocodeModel::QDeclarativeGeocodeModel(QObject *parent)
:   QAbstractListModel(parent), autoUpdate_(false), complete_(false), reply_(0), plugin_(0),
    status_(QDeclarativeGeocodeModel::Null), error_(QDeclarativeGeocodeModel::NoError),
    address_(0), limit_(-1), offset_(0)
{
}

QDeclarativeGeocodeModel::~QDeclarativeGeocodeModel()
{
    qDeleteAll(declarativeLocations_);
    declarativeLocations_.clear();
    delete reply_;
}

/*!
    \internal
    From QQmlParserStatus
*/
void QDeclarativeGeocodeModel::componentComplete()
{
    complete_ = true;
    if (autoUpdate_)
        update();
}

/*!
    \qmlmethod void QtLocation::GeocodeModel::update()

    Instructs the GeocodeModel to update its data. This is most useful
    when \l autoUpdate is disabled, to force a refresh when the query
    has been changed.
*/
void QDeclarativeGeocodeModel::update()
{
    if (!complete_)
        return;

    if (!plugin_) {
        setError(EngineNotSetError, tr("Cannot geocode, plugin not set."));
        return;
    }

    QGeoServiceProvider *serviceProvider = plugin_->sharedGeoServiceProvider();
    if (!serviceProvider)
        return;

    QGeoCodingManager *geocodingManager = serviceProvider->geocodingManager();
    if (!geocodingManager) {
        setError(EngineNotSetError, tr("Cannot geocode, geocode manager not set."));
        return;
    }
    if (!coordinate_.isValid() && (!address_ || address_->address().isEmpty()) &&
        (searchString_.isEmpty())) {
        setError(ParseError, tr("Cannot geocode, valid query not set."));
        return;
    }
    abortRequest(); // abort possible previous requests
    setError(NoError, QString());

    if (coordinate_.isValid()) {
        setStatus(QDeclarativeGeocodeModel::Loading);
        reply_ = geocodingManager->reverseGeocode(coordinate_, boundingArea_);
        if (reply_->isFinished()) {
            if (reply_->error() == QGeoCodeReply::NoError) {
                geocodeFinished(reply_);
            } else {
                geocodeError(reply_, reply_->error(), reply_->errorString());
            }
        }
    } else if (address_) {
        setStatus(QDeclarativeGeocodeModel::Loading);
        reply_ = geocodingManager->geocode(address_->address(), boundingArea_);
        if (reply_->isFinished()) {
            if (reply_->error() == QGeoCodeReply::NoError) {
                geocodeFinished(reply_);
            } else {
                geocodeError(reply_, reply_->error(), reply_->errorString());
            }
        }
    } else if (!searchString_.isEmpty()) {
        setStatus(QDeclarativeGeocodeModel::Loading);
        reply_ = geocodingManager->geocode(searchString_, limit_, offset_, boundingArea_);
        if (reply_->isFinished()) {
            if (reply_->error() == QGeoCodeReply::NoError) {
                geocodeFinished(reply_);
            } else {
                geocodeError(reply_, reply_->error(), reply_->errorString());
            }
        }
    }
}

/*!
    \internal
*/
void QDeclarativeGeocodeModel::abortRequest()
{
    if (reply_) {
        reply_->abort();
        reply_->deleteLater();
        reply_ = 0;
    }
}

/*!
    \internal
*/
void QDeclarativeGeocodeModel::queryContentChanged()
{
    if (autoUpdate_)
        update();
}

/*!
    \internal
    From QAbstractListModel
*/
int QDeclarativeGeocodeModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return declarativeLocations_.count();
}

/*!
    \internal
*/
QVariant QDeclarativeGeocodeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    if (index.row() >= declarativeLocations_.count())
        return QVariant();
    if (role == QDeclarativeGeocodeModel::LocationRole) {
        QObject *locationObject = declarativeLocations_.at(index.row());
        Q_ASSERT(locationObject);
        return QVariant::fromValue(locationObject);
    }
    return QVariant();
}

QHash<int, QByteArray> QDeclarativeGeocodeModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = QAbstractItemModel::roleNames();
    roleNames.insert(LocationRole, "locationData");
    return roleNames;
}

/*!
    \internal
*/
void QDeclarativeGeocodeModel::setPlugin(QDeclarativeGeoServiceProvider *plugin)
{
    if (plugin_ == plugin)
        return;

    reset(); // reset the model
    plugin_ = plugin;
    if (complete_)
        emit pluginChanged();

    if (!plugin)
        return;

    if (plugin_->isAttached()) {
        pluginReady();
    } else {
        connect(plugin_, SIGNAL(attached()),
                this, SLOT(pluginReady()));
    }
}

/*!
    \internal
*/
void QDeclarativeGeocodeModel::pluginReady()
{
    QGeoServiceProvider *serviceProvider = plugin_->sharedGeoServiceProvider();
    QGeoCodingManager *geocodingManager = serviceProvider->geocodingManager();

    if (serviceProvider->error() != QGeoServiceProvider::NoError) {
        QDeclarativeGeocodeModel::GeocodeError newError = UnknownError;
        switch (serviceProvider->error()) {
        case QGeoServiceProvider::NotSupportedError:
            newError = EngineNotSetError; break;
        case QGeoServiceProvider::UnknownParameterError:
            newError = UnknownParameterError; break;
        case QGeoServiceProvider::MissingRequiredParameterError:
            newError = MissingRequiredParameterError; break;
        case QGeoServiceProvider::ConnectionError:
            newError = CommunicationError; break;
        default:
            break;
        }

        setError(newError, serviceProvider->errorString());
        return;
    }

    if (!geocodingManager) {
        setError(EngineNotSetError,tr("Plugin does not support (reverse) geocoding."));
        return;
    }

    connect(geocodingManager, SIGNAL(finished(QGeoCodeReply*)),
            this, SLOT(geocodeFinished(QGeoCodeReply*)));
    connect(geocodingManager, SIGNAL(error(QGeoCodeReply*,QGeoCodeReply::Error,QString)),
            this, SLOT(geocodeError(QGeoCodeReply*,QGeoCodeReply::Error,QString)));
}

/*!
    \qmlproperty Plugin QtLocation::GeocodeModel::plugin

    This property holds the plugin that provides the actual geocoding service.
    Note that all plugins do not necessarily provide geocoding (could for example provide
    only routing or maps).

    \sa Plugin
*/

QDeclarativeGeoServiceProvider *QDeclarativeGeocodeModel::plugin() const
{
    return plugin_;
}

void QDeclarativeGeocodeModel::setBounds(const QVariant &boundingArea)
{
    QGeoShape s;

    if (boundingArea.userType() == qMetaTypeId<QGeoRectangle>())
        s = boundingArea.value<QGeoRectangle>();
    else if (boundingArea.userType() == qMetaTypeId<QGeoCircle>())
        s = boundingArea.value<QGeoCircle>();
    else if (boundingArea.userType() == qMetaTypeId<QGeoShape>())
        s = boundingArea.value<QGeoShape>();


    if (boundingArea_ == s)
        return;

    boundingArea_ = s;
    emit boundsChanged();
}

/*!
    \qmlproperty geoshape QtLocation::GeocodeModel::bounds

    This property holds the bounding area used to limit the results to those
    within the area. This is particularly useful if query is only partially filled out,
    as the service will attempt to (reverse) geocode all matches for the specified data.

    Accepted types are \l {georectangle} and
    \l {geocircle}.
*/
QVariant QDeclarativeGeocodeModel::bounds() const
{
    if (boundingArea_.type() == QGeoShape::RectangleType)
        return QVariant::fromValue(QGeoRectangle(boundingArea_));
    else if (boundingArea_.type() == QGeoShape::CircleType)
        return QVariant::fromValue(QGeoCircle(boundingArea_));
    else
        return QVariant::fromValue(boundingArea_);
}

void QDeclarativeGeocodeModel::geocodeFinished(QGeoCodeReply *reply)
{
    if (reply != reply_ || reply->error() != QGeoCodeReply::NoError)
        return;
    int oldCount = declarativeLocations_.count();
    setLocations(reply->locations());
    setError(NoError, QString());
    setStatus(QDeclarativeGeocodeModel::Ready);
    reply->deleteLater();
    reply_ = 0;
    emit locationsChanged();
    if (oldCount != declarativeLocations_.count())
        emit countChanged();
}

/*!
    \internal
*/
void QDeclarativeGeocodeModel::geocodeError(QGeoCodeReply *reply,
        QGeoCodeReply::Error error,
        const QString &errorString)
{
    if (reply != reply_)
        return;
    Q_UNUSED(error);
    int oldCount = declarativeLocations_.count();
    if (oldCount > 0) {
        // Reset the model
        setLocations(reply->locations());
        emit locationsChanged();
        emit countChanged();
    }
    setError(static_cast<QDeclarativeGeocodeModel::GeocodeError>(error), errorString);
    setStatus(QDeclarativeGeocodeModel::Error);
    reply->deleteLater();
    reply_ = 0;
}

/*!
    \qmlproperty enumeration QtLocation::GeocodeModel::status

    This read-only property holds the current status of the model.

    \list
    \li GeocodeModel.Null - No geocode requests have been issued or \l reset has been called.
    \li GeocodeModel.Ready - Geocode request(s) have finished successfully.
    \li GeocodeModel.Loading - Geocode request has been issued but not yet finished
    \li GeocodeModel.Error - Geocoding error has occurred, details are in \l error and \l errorString
    \endlist
*/

QDeclarativeGeocodeModel::Status QDeclarativeGeocodeModel::status() const
{
    return status_;
}

void QDeclarativeGeocodeModel::setStatus(QDeclarativeGeocodeModel::Status status)
{
    if (status_ == status)
        return;
    status_ = status;
    emit statusChanged();
}

/*!
    \qmlproperty enumeration QtLocation::GeocodeModel::error

    This read-only property holds the latest error value of the geocoding request.

    \list
    \li GeocodeModel.NoError - No error has occurred.
    \li GeocodeModel.CombinationError - An error occurred while results where being combined from multiple sources.
    \li GeocodeModel.CommunicationError - An error occurred while communicating with the service provider.
    \li GeocodeModel.EngineNotSetError - The model's plugin property was not set or there is no geocoding manager associated with the plugin.
    \li GeocodeModel.MissingRequiredParameterError - A required parameter was not specified.
    \li GeocodeModel.ParseError - The response from the service provider was in an unrecognizable format.
    \li GeocodeModel.UnknownError - An error occurred which does not fit into any of the other categories.
    \li GeocodeModel.UnknownParameterError - The plugin did not recognize one of the parameters it was given.
    \li GeocodeModel.UnsupportedOptionError - The requested operation is not supported by the geocoding provider.
                                              This may happen when the loaded engine does not support a particular geocoding request
                                              such as reverse geocoding.
    \endlist
*/

QDeclarativeGeocodeModel::GeocodeError QDeclarativeGeocodeModel::error() const
{
    return error_;
}

void QDeclarativeGeocodeModel::setError(GeocodeError error, const QString &errorString)
{
    if (error_ == error && errorString_ == errorString)
        return;
    error_ = error;
    errorString_ = errorString;
    emit errorChanged();
}

/*!
    \qmlproperty string QtLocation::GeocodeModel::errorString

    This read-only property holds the textual presentation of the latest geocoding error.
    If no error has occurred or the model has been reset, an empty string is returned.

    An empty string may also be returned if an error occurred which has no associated
    textual representation.
*/

QString QDeclarativeGeocodeModel::errorString() const
{
    return errorString_;
}

/*!
    \internal
*/
void QDeclarativeGeocodeModel::setLocations(const QList<QGeoLocation> &locations)
{
    beginResetModel();
    qDeleteAll(declarativeLocations_);
    declarativeLocations_.clear();
    for (int i = 0;  i < locations.count(); ++i) {
        QDeclarativeGeoLocation *location = new QDeclarativeGeoLocation(locations.at(i), this);
        declarativeLocations_.append(location);
    }
    endResetModel();
}

/*!
    \qmlproperty int QtLocation::GeocodeModel::count

    This property holds how many locations the model currently has.
    Amongst other uses, you can use this value when accessing locations
    via the GeocodeModel::get -method.
*/

int QDeclarativeGeocodeModel::count() const
{
    return declarativeLocations_.count();
}

/*!
    \qmlmethod Location QtLocation::GeocodeModel::get(int)

    Returns the Location at given index. Use \l count property to check the
    amount of locations available. The locations are indexed from zero, so the accessible range
    is 0...(count - 1).

    If you access out of bounds, a zero (null object) is returned and a warning is issued.
*/

QDeclarativeGeoLocation *QDeclarativeGeocodeModel::get(int index)
{
    if (index < 0 || index >= declarativeLocations_.count()) {
        qmlInfo(this) << QStringLiteral("Index '%1' out of range").arg(index);
        return 0;
    }
    return declarativeLocations_.at(index);
}

/*!
    \qmlproperty int QtLocation::GeocodeModel::limit

    This property holds the maximum number of results. The limit and \l offset values are only
    applicable with free string geocoding (that is they are not considered when using addresses
    or coordinates in the search query).

    If limit is -1 the entire result set will be returned, otherwise at most limit results will be
    returned.  The limit and \l offset results can be used together to implement paging.
*/

int QDeclarativeGeocodeModel::limit() const
{
    return limit_;
}

void QDeclarativeGeocodeModel::setLimit(int limit)
{
    if (limit == limit_)
        return;
    limit_ = limit;
    if (autoUpdate_) {
        update();
    }
    emit limitChanged();
}

/*!
    \qmlproperty int QtLocation::GeocodeModel::offset

    This property tells not to return the first 'offset' number of the results. The \l limit and
    offset values are only applicable with free string geocoding (that is they are not considered
    when using addresses or coordinates in the search query).

    The \l limit and offset results can be used together to implement paging.
*/

int QDeclarativeGeocodeModel::offset() const
{
    return offset_;
}

void QDeclarativeGeocodeModel::setOffset(int offset)
{
    if (offset == offset_)
        return;
    offset_ = offset;
    if (autoUpdate_) {
        update();
    }
    emit offsetChanged();
}

/*!
    \qmlmethod void QtLocation::GeocodeModel::reset()

    Resets the model. All location data is cleared, any outstanding requests
    are aborted and possible errors are cleared. Model status will be set
    to GeocodeModel.Null
*/

void QDeclarativeGeocodeModel::reset()
{
    beginResetModel();
    if (!declarativeLocations_.isEmpty()) {
        setLocations(QList<QGeoLocation>());
        emit countChanged();
    }
    endResetModel();

    abortRequest();
    setError(NoError, QString());
    setStatus(QDeclarativeGeocodeModel::Null);
}

/*!
    \qmlmethod void QtLocation::GeocodeModel::cancel()

    Cancels any outstanding requests and clears errors.  Model status will be set to either
    GeocodeModel.Null or GeocodeModel.Ready.
*/
void QDeclarativeGeocodeModel::cancel()
{
    abortRequest();
    setError(NoError, QString());
    setStatus(declarativeLocations_.isEmpty() ? Null : Ready);
}

/*!
    \qmlproperty QVariant QtLocation::GeocodeModel::query

    This property holds the data of the geocoding request.
    The property accepts three types of queries which determine both the data and
    the type of action to be performed:

    \list
    \li Address - Geocoding (address to coordinate)
    \li \l {coordinate} - Reverse geocoding (coordinate to address)
    \li string - Geocoding (address to coordinate)
    \endlist
*/

QVariant QDeclarativeGeocodeModel::query() const
{
    return queryVariant_;
}

void QDeclarativeGeocodeModel::setQuery(const QVariant &query)
{
    if (query == queryVariant_)
        return;

    if (query.userType() == qMetaTypeId<QGeoCoordinate>()) {
        if (address_) {
            address_->disconnect(this);
            address_ = 0;
        }
        searchString_.clear();

        coordinate_ = query.value<QGeoCoordinate>();
    } else if (query.type() == QVariant::String) {
        searchString_ = query.toString();
        if (address_) {
            address_->disconnect(this);
            address_ = 0;
        }
        coordinate_ = QGeoCoordinate();
    } else if (QObject *object = query.value<QObject *>()) {
        if (QDeclarativeGeoAddress *address = qobject_cast<QDeclarativeGeoAddress *>(object)) {
            if (address_)
                address_->disconnect(this);
            coordinate_ = QGeoCoordinate();
            searchString_.clear();

            address_ = address;
            connect(address_, SIGNAL(countryChanged()), this, SLOT(queryContentChanged()));
            connect(address_, SIGNAL(countryCodeChanged()), this, SLOT(queryContentChanged()));
            connect(address_, SIGNAL(stateChanged()), this, SLOT(queryContentChanged()));
            connect(address_, SIGNAL(countyChanged()), this, SLOT(queryContentChanged()));
            connect(address_, SIGNAL(cityChanged()), this, SLOT(queryContentChanged()));
            connect(address_, SIGNAL(districtChanged()), this, SLOT(queryContentChanged()));
            connect(address_, SIGNAL(streetChanged()), this, SLOT(queryContentChanged()));
            connect(address_, SIGNAL(postalCodeChanged()), this, SLOT(queryContentChanged()));
        } else {
            qmlInfo(this) << QStringLiteral("Unsupported query type for geocode model ")
                          << QStringLiteral("(coordinate, string and Address supported).");
            return;
        }
    } else {
        qmlInfo(this) << QStringLiteral("Unsupported query type for geocode model ")
                      << QStringLiteral("(coordinate, string and Address supported).");
        return;
    }

    queryVariant_ = query;
    emit queryChanged();
    if (autoUpdate_)
        update();
}

/*!
    \qmlproperty bool QtLocation::GeocodeModel::autoUpdate

    This property controls whether the Model automatically updates in response
    to changes in its attached query. The default value of this property
    is false.

    If setting this value to 'true' and using an Address or
    \l {coordinate} as the query, note that any change at all in the
    object's properties will trigger a new request to be sent. If you are adjusting many
    properties of the object whilst autoUpdate is enabled, this can generate large numbers of
    useless (and later discarded) requests.
*/

bool QDeclarativeGeocodeModel::autoUpdate() const
{
    return autoUpdate_;
}

void QDeclarativeGeocodeModel::setAutoUpdate(bool update)
{
    if (autoUpdate_ == update)
        return;
    autoUpdate_ = update;
    emit autoUpdateChanged();
}

#include "moc_qdeclarativegeocodemodel_p.cpp"

QT_END_NAMESPACE
