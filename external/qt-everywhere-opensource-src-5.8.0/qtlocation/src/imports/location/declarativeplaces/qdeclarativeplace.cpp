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

#include "qdeclarativeplace_p.h"
#include "qdeclarativecontactdetail_p.h"
#include "qdeclarativegeoserviceprovider_p.h"
#include "qdeclarativeplaceattribute_p.h"
#include "qdeclarativeplaceicon_p.h"
#include "error_messages.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlInfo>
#include <QtLocation/QGeoServiceProvider>
#include <QtLocation/QPlaceManager>
#include <QtLocation/QPlaceDetailsReply>
#include <QtLocation/QPlaceReply>
#include <QtLocation/QPlaceIdReply>
#include <QtLocation/QPlaceContactDetail>

QT_USE_NAMESPACE

/*!
    \qmltype Place
    \instantiates QDeclarativePlace
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-places
    \ingroup qml-QtLocation5-places-data
    \since Qt Location 5.5

    \brief The Place type represents a location that is a position of interest.

    The Place type represents a physical location with additional metadata describing that
    location.  Contrasted with \l Location, \l Address, and
    \l {coordinate} type which are used to describe where a location is.
    The basic properties of a Place are its \l name and \l location.

    Place objects are typically obtained from a search model and will generally only have their
    basic properties set. The \l detailsFetched property can be used to test if further property
    values need to be fetched from the \l Plugin. This can be done by invoking the \l getDetails()
    method.  Progress of the fetching operation can be monitored with the \l status property, which
    will be set to Place.Fetching when the details are being fetched.

    The Place type has many properties holding information about the location. Details on how to
    contact the place are available from the \l contactDetails property.  Convenience properties
    for obtaining the primary \l {primaryPhone}{phone}, \l {primaryFax}{fax},
    \l {primaryEmail}{email} and \l {primaryWebsite}{website} are also available.

    Each place is assigned zero or more \l categories.  Categories are typically used when
    searching for a particular kind of place, such as a restaurant or hotel.  Some places have a
    \l ratings object, which gives an indication of the quality of the place.

    Place metadata is provided by a \l supplier who may require that an \l attribution message be
    displayed to the user when the place details are viewed.

    Places have an associated \l icon which can be used to represent a place on a map or to
    decorate a delegate in a view.

    Places may have additional rich content associated with them.  The currently supported rich
    content include editorial descriptions, reviews and images.  These are exposed as a set of
    models for retrieving the content.  Editorial descriptions of the place are available from the
    \l editorialModel property.  Reviews of the place are available from the \l reviewModel
    property.  A gallery of pictures of the place can be accessed using the \l imageModel property.

    Places may have additional attributes which are not covered in the formal API.  The
    \l extendedAttributes property provides access to these.  The type of extended attributes
    available is specific to each \l Plugin.

    A Place is almost always tied to a \l plugin.  The \l plugin property must be set before it is
    possible to call \l save(), \l remove() or \l getDetails().  The \l reviewModel, \l imageModel
    and \l editorialModel are only valid then the \l plugin property is set.

    \section2 Saving a Place

    If the \l Plugin supports it, the Place type can be used to save a place.  First create a new
    Place and set its properties:

    \snippet declarative/places.qml Place savePlace def

    Then invoke the \l save() method:

    \snippet declarative/places.qml Place savePlace

    The \l status property will change to Place.Saving and then to Place.Ready if the save was
    successful or to Place.Error if an error occurs.

    If the \l placeId property is set, the backend will update an existing place otherwise it will
    create a new place.  On success the \l placeId property will be updated with the identifier of the newly
    saved place.

    \section3 Caveats
    \input place-caveats.qdocinc

    \section3 Saving Between Plugins
    When saving places between plugins, there are a few things to be aware of.
    Some fields of a place such as the id, categories and icons are plugin specific entities. For example
    the categories in one manager may not be recognised in another.
    Therefore trying to save a place directly from one plugin to another is not possible.

    It is generally recommended that saving across plugins be handled as saving \l {Favorites}{favorites}
    as explained in the Favorites section.  However there is another approach which is to create a new place,
    set its (destination) plugin and then use the \l copyFrom() method to copy the details of the original place.
    Using \l copyFrom() only copies data that is supported by the destination plugin,
    plugin specific data such as the place identifier is not copied over. Once the copy is done,
    the place is in a suitable state to be saved.

    The following snippet provides an example of saving a place to a different plugin
    using the \l copyFrom method:

    \snippet declarative/places.qml Place save to different plugin

    \section2 Removing a Place

    To remove a place, ensure that a Place object with a valid \l placeId property exists and call
    its \l remove() method.  The \l status property will change to Place.Removing and then to
    Place.Ready if the save was successful or to Place.Error if an error occurs.

    \section2 Favorites
    The Places API supports the concept of favorites. Favorites are generally implemented
    by using two plugins, the first plugin is typically a read-only source of places (origin plugin) and a second
    read/write plugin (destination plugin) is used to store places from the origin as favorites.

    Each Place has a favorite property which is intended to contain the corresponding place
    from the destination plugin (the place itself is sourced from the origin plugin).  Because both the original
    place and favorite instances are available, the developer can choose which
    properties to show to the user. For example the favorite may have a modified name which should
    be displayed rather than the original name.

    \snippet declarative/places.qml Place favorite

    The following demonstrates how to save a new favorite instance.  A call is made
    to create/initialize the favorite instance and then the instance is saved.

    \snippet declarative/places.qml Place saveFavorite

    The following demonstrates favorite removal:

    \snippet declarative/places.qml Place removeFavorite 1
    \dots
    \snippet declarative/places.qml Place removeFavorite 2

    The PlaceSearchModel has a favoritesPlugin property.  If the property is set, any places found
    during a search are checked against the favoritesPlugin to see if there is a corresponding
    favorite place.  If so, the favorite property of the Place is set, otherwise the favorite
    property is remains null.

    \sa PlaceSearchModel
*/

QDeclarativePlace::QDeclarativePlace(QObject *parent)
:   QObject(parent), m_location(0), m_ratings(0), m_supplier(0), m_icon(0),
    m_reviewModel(0), m_imageModel(0), m_editorialModel(0),
    m_extendedAttributes(new QQmlPropertyMap(this)),
    m_contactDetails(new QDeclarativeContactDetails(this)), m_reply(0), m_plugin(0),
    m_complete(false), m_favorite(0), m_status(QDeclarativePlace::Ready)
{
    connect(m_contactDetails, SIGNAL(valueChanged(QString,QVariant)),
            this, SLOT(contactsModified(QString,QVariant)));

    setPlace(QPlace());
}

QDeclarativePlace::QDeclarativePlace(const QPlace &src, QDeclarativeGeoServiceProvider *plugin, QObject *parent)
:   QObject(parent), m_location(0), m_ratings(0), m_supplier(0), m_icon(0),
    m_reviewModel(0), m_imageModel(0), m_editorialModel(0),
    m_extendedAttributes(new QQmlPropertyMap(this)),
    m_contactDetails(new QDeclarativeContactDetails(this)), m_reply(0), m_plugin(plugin),
    m_complete(false), m_favorite(0), m_status(QDeclarativePlace::Ready)
{
    Q_ASSERT(plugin);

    connect(m_contactDetails, SIGNAL(valueChanged(QString,QVariant)),
            this, SLOT(contactsModified(QString,QVariant)));

    setPlace(src);
}

QDeclarativePlace::~QDeclarativePlace()
{
}

// From QQmlParserStatus
void QDeclarativePlace::componentComplete()
{
    m_complete = true;
}

/*!
    \qmlproperty Plugin Place::plugin

    This property holds the \l Plugin that provided this place which can be used to retrieve more information about the service.
*/
void QDeclarativePlace::setPlugin(QDeclarativeGeoServiceProvider *plugin)
{
    if (m_plugin == plugin)
        return;

    m_plugin = plugin;
    if (m_complete)
        emit pluginChanged();

    if (m_plugin->isAttached()) {
        pluginReady();
    } else {
        connect(m_plugin, SIGNAL(attached()),
                this, SLOT(pluginReady()));
    }
}

void QDeclarativePlace::pluginReady()
{
    QGeoServiceProvider *serviceProvider = m_plugin->sharedGeoServiceProvider();
    QPlaceManager *placeManager = serviceProvider->placeManager();
    if (!placeManager || serviceProvider->error() != QGeoServiceProvider::NoError) {
        setStatus(Error, QCoreApplication::translate(CONTEXT_NAME, PLUGIN_ERROR)
                         .arg(m_plugin->name()).arg(serviceProvider->errorString()));
        return;
    }
}

QDeclarativeGeoServiceProvider *QDeclarativePlace::plugin() const
{
    return m_plugin;
}

/*!
    \qmlproperty ReviewModel Place::reviewModel

    This property holds a model which can be used to retrieve reviews about the place.
*/
QDeclarativeReviewModel *QDeclarativePlace::reviewModel()
{
    if (!m_reviewModel) {
        m_reviewModel = new QDeclarativeReviewModel(this);
        m_reviewModel->setPlace(this);
    }

    return m_reviewModel;
}

/*!
    \qmlproperty ImageModel Place::imageModel

    This property holds a model which can be used to retrieve images of the place.
*/
QDeclarativePlaceImageModel *QDeclarativePlace::imageModel()
{
    if (!m_imageModel) {
        m_imageModel = new QDeclarativePlaceImageModel(this);
        m_imageModel->setPlace(this);
    }

    return m_imageModel;
}

/*!
    \qmlproperty EditorialModel Place::editorialModel

    This property holds a model which can be used to retrieve editorial descriptions of the place.
*/
QDeclarativePlaceEditorialModel *QDeclarativePlace::editorialModel()
{
    if (!m_editorialModel) {
        m_editorialModel = new QDeclarativePlaceEditorialModel(this);
        m_editorialModel->setPlace(this);
    }

    return m_editorialModel;
}

/*!
    \qmlproperty QPlace Place::place

    For details on how to use this property to interface between C++ and QML see
    "\l {Place - QPlace} {Interfaces between C++ and QML Code}".
*/
void QDeclarativePlace::setPlace(const QPlace &src)
{
    QPlace previous = m_src;
    m_src = src;

    if (previous.categories() != m_src.categories()) {
        synchronizeCategories();
        emit categoriesChanged();
    }

    if (m_location && m_location->parent() == this) {
        m_location->setLocation(m_src.location());
    } else if (!m_location || m_location->parent() != this) {
        m_location = new QDeclarativeGeoLocation(m_src.location(), this);
        emit locationChanged();
    }

    if (m_ratings && m_ratings->parent() == this) {
        m_ratings->setRatings(m_src.ratings());
    } else if (!m_ratings || m_ratings->parent() != this) {
        m_ratings = new QDeclarativeRatings(m_src.ratings(), this);
        emit ratingsChanged();
    }

    if (m_supplier && m_supplier->parent() == this) {
        m_supplier->setSupplier(m_src.supplier(), m_plugin);
    } else if (!m_supplier || m_supplier->parent() != this) {
        m_supplier = new QDeclarativeSupplier(m_src.supplier(), m_plugin, this);
        emit supplierChanged();
    }

    if (m_icon && m_icon->parent() == this) {
        m_icon->setPlugin(m_plugin);
        m_icon->setIcon(m_src.icon());
    } else if (!m_icon || m_icon->parent() != this) {
        m_icon = new QDeclarativePlaceIcon(m_src.icon(), m_plugin, this);
        emit iconChanged();
    }

    if (previous.name() != m_src.name()) {
        emit nameChanged();
    }
    if (previous.placeId() != m_src.placeId()) {
        emit placeIdChanged();
    }
    if (previous.attribution() != m_src.attribution()) {
        emit attributionChanged();
    }
    if (previous.detailsFetched() != m_src.detailsFetched()) {
        emit detailsFetchedChanged();
    }
    if (previous.primaryPhone() != m_src.primaryPhone()) {
        emit primaryPhoneChanged();
    }
    if (previous.primaryFax() != m_src.primaryFax()) {
        emit primaryFaxChanged();
    }
    if (previous.primaryEmail() != m_src.primaryEmail()) {
        emit primaryEmailChanged();
    }
    if (previous.primaryWebsite() != m_src.primaryWebsite()) {
        emit primaryWebsiteChanged();
    }

    if (m_reviewModel && m_src.totalContentCount(QPlaceContent::ReviewType) >= 0) {
        m_reviewModel->initializeCollection(m_src.totalContentCount(QPlaceContent::ReviewType),
                                            m_src.content(QPlaceContent::ReviewType));
    }
    if (m_imageModel && m_src.totalContentCount(QPlaceContent::ImageType) >= 0) {
        m_imageModel->initializeCollection(m_src.totalContentCount(QPlaceContent::ImageType),
                                           m_src.content(QPlaceContent::ImageType));
    }
    if (m_editorialModel && m_src.totalContentCount(QPlaceContent::EditorialType) >= 0) {
        m_editorialModel->initializeCollection(m_src.totalContentCount(QPlaceContent::EditorialType),
                                               m_src.content(QPlaceContent::EditorialType));
    }

    synchronizeExtendedAttributes();
    synchronizeContacts();
}

QPlace QDeclarativePlace::place()
{
    // The following properties are not stored in m_src but instead stored in QDeclarative* objects

    QPlace result = m_src;

    // Categories
    QList<QPlaceCategory> categories;
    foreach (QDeclarativeCategory *value, m_categories)
        categories.append(value->category());

    result.setCategories(categories);

    // Location
    result.setLocation(m_location ? m_location->location() : QGeoLocation());

    // Rating
    result.setRatings(m_ratings ? m_ratings->ratings() : QPlaceRatings());

    // Supplier
    result.setSupplier(m_supplier ? m_supplier->supplier() : QPlaceSupplier());

    // Icon
    result.setIcon(m_icon ? m_icon->icon() : QPlaceIcon());

    //contact details
    QList<QPlaceContactDetail> cppDetails;
    foreach (const QString &key, m_contactDetails->keys()) {
        cppDetails.clear();
        if (m_contactDetails->value(key).type() == QVariant::List) {
            QVariantList detailsVarList = m_contactDetails->value(key).toList();
            foreach (const QVariant &detailVar, detailsVarList) {
                QDeclarativeContactDetail *detail = qobject_cast<QDeclarativeContactDetail *>(detailVar.value<QObject *>());
                if (detail)
                    cppDetails.append(detail->contactDetail());
            }
        } else {
            QDeclarativeContactDetail *detail = qobject_cast<QDeclarativeContactDetail *>(m_contactDetails->value(key).value<QObject *>());
            if (detail)
                cppDetails.append(detail->contactDetail());
        }
        result.setContactDetails(key, cppDetails);
    }

    return result;
}

/*!
    \qmlproperty QtPositioning::Location Place::location

    This property holds the location of the place which can be used to retrieve the coordinate,
    address and the bounding box.
*/
void QDeclarativePlace::setLocation(QDeclarativeGeoLocation *location)
{
    if (m_location == location)
        return;

    if (m_location && m_location->parent() == this)
        delete m_location;

    m_location = location;
    emit locationChanged();
}

QDeclarativeGeoLocation *QDeclarativePlace::location()
{
    return m_location;
}

/*!
    \qmlproperty Ratings Place::ratings

    This property holds ratings of the place.  The ratings provide an indication of the quality of a
    place.
*/
void QDeclarativePlace::setRatings(QDeclarativeRatings *rating)
{
    if (m_ratings == rating)
        return;

    if (m_ratings && m_ratings->parent() == this)
        delete m_ratings;

    m_ratings = rating;
    emit ratingsChanged();
}

QDeclarativeRatings *QDeclarativePlace::ratings()
{

    return m_ratings;
}

/*!
    \qmlproperty Supplier Place::supplier

    This property holds the supplier of the place data.
    The supplier is typically a business or organization that collected the data about the place.
*/
void QDeclarativePlace::setSupplier(QDeclarativeSupplier *supplier)
{
    if (m_supplier == supplier)
        return;

    if (m_supplier && m_supplier->parent() == this)
        delete m_supplier;

    m_supplier = supplier;
    emit supplierChanged();
}

QDeclarativeSupplier *QDeclarativePlace::supplier() const
{
    return m_supplier;
}

/*!
    \qmlproperty Icon Place::icon

    This property holds a graphical icon which can be used to represent the place.
*/
QDeclarativePlaceIcon *QDeclarativePlace::icon() const
{
    return m_icon;
}

void QDeclarativePlace::setIcon(QDeclarativePlaceIcon *icon)
{
    if (m_icon == icon)
        return;

    if (m_icon && m_icon->parent() == this)
        delete m_icon;

    m_icon = icon;
    emit iconChanged();
}

/*!
    \qmlproperty string Place::name

    This property holds the name of the place which can be used to represent the place.
*/
void QDeclarativePlace::setName(const QString &name)
{
    if (m_src.name() != name) {
        m_src.setName(name);
        emit nameChanged();
    }
}

QString QDeclarativePlace::name() const
{
    return m_src.name();
}

/*!
    \qmlproperty string Place::placeId

    This property holds the unique identifier of the place.  The place identifier is only meaningful to the
    \l Plugin that generated it and is not transferable between \l {Plugin}{Plugins}.  The place id
    is not guaranteed to be universally unique, but unique within the \l Plugin that generated it.

    If only the place identifier is known, all other place data can fetched from the \l Plugin.

    \snippet declarative/places.qml Place placeId
*/
void QDeclarativePlace::setPlaceId(const QString &placeId)
{
    if (m_src.placeId() != placeId) {
        m_src.setPlaceId(placeId);
        emit placeIdChanged();
    }
}

QString QDeclarativePlace::placeId() const
{
    return m_src.placeId();
}

/*!
    \qmlproperty string Place::attribution

    This property holds a rich text attribution string for the place.
    Some providers may require that the attribution be shown to the user
    whenever a place is displayed.  The contents of this property should
    be shown to the user if it is not empty.
*/
void QDeclarativePlace::setAttribution(const QString &attribution)
{
    if (m_src.attribution() != attribution) {
        m_src.setAttribution(attribution);
        emit attributionChanged();
    }
}

QString QDeclarativePlace::attribution() const
{
    return m_src.attribution();
}

/*!
    \qmlproperty bool Place::detailsFetched

    This property indicates whether the details of the place have been fetched.  If this property
    is false, the place details have not yet been fetched.  Fetching can be done by invoking the
    \l getDetails() method.

    \sa getDetails()
*/
bool QDeclarativePlace::detailsFetched() const
{
    return m_src.detailsFetched();
}

/*!
    \qmlproperty enumeration Place::status

    This property holds the status of the place.  It can be one of:

    \table
        \row
            \li Place.Ready
            \li No error occurred during the last operation, further operations may be performed on
               the place.
        \row
            \li Place.Saving
            \li The place is currently being saved, no other operation may be performed until
               complete.
        \row
            \li Place.Fetching
            \li The place details are currently being fetched, no other operations may be performed
               until complete.
        \row
            \li Place.Removing
            \li The place is currently being removed, no other operations can be performed until
               complete.
        \row
            \li Place.Error
            \li An error occurred during the last operation, further operations can still be
               performed on the place.
    \endtable

    The status of a place can be checked by connecting the status property
    to a handler function, and then have the handler function process the change
    in status.

    \snippet declarative/places.qml Place checkStatus
    \dots
    \snippet declarative/places.qml Place checkStatus handler

*/
void QDeclarativePlace::setStatus(Status status, const QString &errorString)
{
    Status originalStatus = m_status;
    m_status = status;
    m_errorString = errorString;

    if (originalStatus != m_status)
        emit statusChanged();
}

QDeclarativePlace::Status QDeclarativePlace::status() const
{
    return m_status;
}

/*!
    \internal
*/
void QDeclarativePlace::finished()
{
    if (!m_reply)
        return;

    if (m_reply->error() == QPlaceReply::NoError) {
        switch (m_reply->type()) {
        case (QPlaceReply::IdReply) : {
            QPlaceIdReply *idReply = qobject_cast<QPlaceIdReply *>(m_reply);

            switch (idReply->operationType()) {
            case QPlaceIdReply::SavePlace:
                setPlaceId(idReply->id());
                break;
            case QPlaceIdReply::RemovePlace:
                break;
            default:
                //Other operation types shouldn't ever be received.
                break;
            }
            break;
        }
        case (QPlaceReply::DetailsReply): {
            QPlaceDetailsReply *detailsReply = qobject_cast<QPlaceDetailsReply *>(m_reply);
            setPlace(detailsReply->place());
            break;
        }
        default:
            //other types of replies shouldn't ever be received.
            break;
        }

        m_errorString.clear();

        m_reply->deleteLater();
        m_reply = 0;

        setStatus(QDeclarativePlace::Ready);
    } else {
        QString errorString = m_reply->errorString();

        m_reply->deleteLater();
        m_reply = 0;

        setStatus(QDeclarativePlace::Error, errorString);
    }
}

/*!
    \internal
*/
void QDeclarativePlace::contactsModified(const QString &key, const QVariant &)
{
    primarySignalsEmission(key);
}

/*!
    \internal
*/
void QDeclarativePlace::cleanupDeletedCategories()
{
    foreach (QDeclarativeCategory * category, m_categoriesToBeDeleted) {
        if (category->parent() == this)
            delete category;
    }
    m_categoriesToBeDeleted.clear();
}

/*!
    \qmlmethod void Place::getDetails()

    This method starts fetching place details.

    The \l status property will change to Place.Fetching while the fetch is in progress.  On
    success the object's properties will be updated, \l status will be set to Place.Ready and
    \l detailsFetched will be set to true.  On error \l status will be set to Place.Error.  The
    \l errorString() method can be used to get the details of the error.
*/
void QDeclarativePlace::getDetails()
{
    QPlaceManager *placeManager = manager();
    if (!placeManager)
        return;

    m_reply = placeManager->getPlaceDetails(placeId());
    connect(m_reply, SIGNAL(finished()), this, SLOT(finished()));
    setStatus(QDeclarativePlace::Fetching);
}

/*!
    \qmlmethod void Place::save()

    This method performs a save operation on the place.

    The \l status property will change to Place.Saving while the save operation is in progress.  On
    success the \l status will be set to Place.Ready.  On error \l status will be set to Place.Error.
    The \l errorString() method can be used to get the details of the error.

    If the \l placeId property was previously empty, it will be assigned a valid value automatically
    during a successful save operation.

    Note that a \l PlaceSearchModel will call Place::getDetails on any place that it detects an update
    on.  A consequence of this is that whenever a Place from a \l PlaceSearchModel is successfully saved,
    it will be followed by a fetch of place details, leading to a sequence of state changes
    of \c Saving, \c Ready, \c Fetching, \c Ready.

*/
void QDeclarativePlace::save()
{
    QPlaceManager *placeManager = manager();
    if (!placeManager)
        return;

    m_reply = placeManager->savePlace(place());
    connect(m_reply, SIGNAL(finished()), this, SLOT(finished()));
    setStatus(QDeclarativePlace::Saving);
}

/*!
    \qmlmethod void Place::remove()

    This method performs a remove operation on the place.

    The \l status property will change to Place.Removing while the save operation is in progress.
    On success \l status will be set to Place.Ready.  On error \l status will be set to
    Place.Error.  The \l errorString() method can be used to get the details of the error.
*/
void QDeclarativePlace::remove()
{
    QPlaceManager *placeManager = manager();
    if (!placeManager)
        return;

    m_reply = placeManager->removePlace(place().placeId());
    connect(m_reply, SIGNAL(finished()), this, SLOT(finished()));
    setStatus(QDeclarativePlace::Removing);
}

/*!
    \qmlmethod string Place::errorString()

    Returns a string description of the error of the last operation.  If the last operation
    completed successfully then the string is empty.
*/
QString QDeclarativePlace::errorString() const
{
    return m_errorString;
}

/*!
    \qmlproperty string Place::primaryPhone

    This property holds the primary phone number of the place.  If no "phone" contact detail is
    defined for this place, this property will be an empty string.  It is equivalent to:


    \snippet declarative/places.qml Place primaryPhone
*/
QString QDeclarativePlace::primaryPhone() const
{
    return primaryValue(QPlaceContactDetail::Phone);
}

/*!
    \qmlproperty string Place::primaryFax

    This property holds the primary fax number of the place.  If no "fax" contact detail is
    defined for this place this property will be an empty string.  It is equivalent to

    \snippet declarative/places.qml Place primaryFax
*/
QString QDeclarativePlace::primaryFax() const
{
    return primaryValue(QPlaceContactDetail::Fax);
}

/*!
    \qmlproperty string Place::primaryEmail

    This property holds the primary email address of the place.  If no "email" contact detail is
    defined for this place this property will be an empty string.  It is equivalent to

    \snippet declarative/places.qml Place primaryEmail
*/
QString QDeclarativePlace::primaryEmail() const
{
    return primaryValue(QPlaceContactDetail::Email);
}

/*!
    \qmlproperty string Place::primaryWebsite

    This property holds the primary website url of the place.  If no "website" contact detail is
    defined for this place this property will be an empty string.  It is equivalent to

    \snippet declarative/places.qml Place primaryWebsite
*/

QUrl QDeclarativePlace::primaryWebsite() const
{
    return QUrl(primaryValue(QPlaceContactDetail::Website));
}

/*!
    \qmlproperty ExtendedAttributes Place::extendedAttributes

    This property holds the extended attributes of a place.  Extended attributes are additional
    information about a place not covered by the place's properties.
*/
QQmlPropertyMap *QDeclarativePlace::extendedAttributes() const
{
    return m_extendedAttributes;
}

/*!
    \qmlproperty ContactDetails Place::contactDetails

    This property holds the contact information for this place, for example a phone number or
    a website URL.  This property is a map of \l ContactDetail objects.
*/
QDeclarativeContactDetails *QDeclarativePlace::contactDetails() const
{
    return m_contactDetails;
}

/*!
    \qmlproperty list<Category> Place::categories

    This property holds the list of categories this place is a member of.  The categories that can
    be assigned to a place are specific to each \l plugin.
*/
QQmlListProperty<QDeclarativeCategory> QDeclarativePlace::categories()
{
    return QQmlListProperty<QDeclarativeCategory>(this,
                                                          0, // opaque data parameter
                                                          category_append,
                                                          category_count,
                                                          category_at,
                                                          category_clear);
}

/*!
    \internal
*/
void QDeclarativePlace::category_append(QQmlListProperty<QDeclarativeCategory> *prop,
                                                  QDeclarativeCategory *value)
{
    QDeclarativePlace *object = static_cast<QDeclarativePlace *>(prop->object);

    if (object->m_categoriesToBeDeleted.contains(value))
        object->m_categoriesToBeDeleted.removeAll(value);

    if (!object->m_categories.contains(value)) {
        object->m_categories.append(value);
        QList<QPlaceCategory> list = object->m_src.categories();
        list.append(value->category());
        object->m_src.setCategories(list);

        emit object->categoriesChanged();
    }
}

/*!
    \internal
*/
int QDeclarativePlace::category_count(QQmlListProperty<QDeclarativeCategory> *prop)
{
    return static_cast<QDeclarativePlace *>(prop->object)->m_categories.count();
}

/*!
    \internal
*/
QDeclarativeCategory *QDeclarativePlace::category_at(QQmlListProperty<QDeclarativeCategory> *prop,
                                                                          int index)
{
    QDeclarativePlace *object = static_cast<QDeclarativePlace *>(prop->object);
    QDeclarativeCategory *res = NULL;
    if (object->m_categories.count() > index && index > -1) {
        res = object->m_categories[index];
    }
    return res;
}

/*!
    \internal
*/
void QDeclarativePlace::category_clear(QQmlListProperty<QDeclarativeCategory> *prop)
{
    QDeclarativePlace *object = static_cast<QDeclarativePlace *>(prop->object);
    if (object->m_categories.isEmpty())
        return;

    for (int i = 0; i < object->m_categories.count(); ++i) {
        if (object->m_categories.at(i)->parent() == object)
            object->m_categoriesToBeDeleted.append(object->m_categories.at(i));
    }

    object->m_categories.clear();
    object->m_src.setCategories(QList<QPlaceCategory>());
    emit object->categoriesChanged();
    QMetaObject::invokeMethod(object, "cleanupDeletedCategories", Qt::QueuedConnection);
}

/*!
    \internal
*/
void QDeclarativePlace::synchronizeCategories()
{
    qDeleteAll(m_categories);
    m_categories.clear();
    foreach (const QPlaceCategory &value, m_src.categories()) {
        QDeclarativeCategory *declarativeValue = new QDeclarativeCategory(value, m_plugin, this);
        m_categories.append(declarativeValue);
    }
}

/*!
    \qmlproperty enumeration Place::visibility

    This property holds the visibility of the place.  It can be one of:

    \table
        \row
            \li Place.UnspecifiedVisibility
            \li The visibility of the place is unspecified, the default visibility of the \l Plugin
               will be used.
        \row
            \li Place.DeviceVisibility
            \li The place is limited to the current device.  The place will not be transferred off
               of the device.
        \row
            \li Place.PrivateVisibility
            \li The place is private to the current user.  The place may be transferred to an online
               service but is only ever visible to the current user.
        \row
            \li Place.PublicVisibility
            \li The place is public.
    \endtable

    Note that visibility does not affect how the place is displayed
    in the user-interface of an application on the device.  Instead,
    it defines the sharing semantics of the place.
*/
QDeclarativePlace::Visibility QDeclarativePlace::visibility() const
{
    return static_cast<QDeclarativePlace::Visibility>(m_src.visibility());
}

void QDeclarativePlace::setVisibility(Visibility visibility)
{
    if (static_cast<QDeclarativePlace::Visibility>(m_src.visibility()) == visibility)
        return;

    m_src.setVisibility(static_cast<QLocation::Visibility>(visibility));
    emit visibilityChanged();
}

/*!
    \qmlproperty Place Place::favorite

    This property holds the favorite instance of a place.
*/
QDeclarativePlace *QDeclarativePlace::favorite() const
{
    return m_favorite;
}

void QDeclarativePlace::setFavorite(QDeclarativePlace *favorite)
{

    if (m_favorite == favorite)
        return;

    if (m_favorite && m_favorite->parent() == this)
        delete m_favorite;

    m_favorite = favorite;
    emit favoriteChanged();
}

/*!
    \qmlmethod void Place::copyFrom(Place original)

    Copies data from an \a original place into this place.  Only data that is supported by this
    place's plugin is copied over and plugin specific data such as place identifier is not copied over.
*/
void QDeclarativePlace::copyFrom(QDeclarativePlace *original)
{
    QPlaceManager *placeManager = manager();
    if (!placeManager)
        return;

    setPlace(placeManager->compatiblePlace(original->place()));
}

/*!
    \qmlmethod void Place::initializeFavorite(Plugin destinationPlugin)

    Creates a favorite instance for the place which is to be saved into the
    \a destination plugin.  This method does nothing if the favorite property is
    not null.
*/
void QDeclarativePlace::initializeFavorite(QDeclarativeGeoServiceProvider *plugin)
{
    if (m_favorite == 0) {
        QDeclarativePlace *place = new QDeclarativePlace(this);
        place->setPlugin(plugin);
        place->copyFrom(this);
        setFavorite(place);
    }
}

/*!
    \internal
*/
void QDeclarativePlace::synchronizeExtendedAttributes()
{
    QStringList keys = m_extendedAttributes->keys();
    foreach (const QString &key, keys)
        m_extendedAttributes->clear(key);

    QStringList attributeTypes = m_src.extendedAttributeTypes();
    foreach (const QString &attributeType, attributeTypes) {
        m_extendedAttributes->insert(attributeType,
            qVariantFromValue(new QDeclarativePlaceAttribute(m_src.extendedAttribute(attributeType))));
    }

    emit extendedAttributesChanged();
}

/*!
    \internal
*/
void QDeclarativePlace::synchronizeContacts()
{
    //clear out contact data
    foreach (const QString &contactType, m_contactDetails->keys()) {
        QList<QVariant> contacts = m_contactDetails->value(contactType).toList();
        foreach (const QVariant &var, contacts) {
            QObject *obj = var.value<QObject *>();
            if (obj->parent() == this)
                delete obj;
        }
        m_contactDetails->insert(contactType, QVariantList());
    }

    //insert new contact data from source place
    foreach (const QString &contactType, m_src.contactTypes()) {
        QList<QPlaceContactDetail> sourceContacts = m_src.contactDetails(contactType);
        QVariantList declContacts;
        foreach (const QPlaceContactDetail &sourceContact, sourceContacts) {
            QDeclarativeContactDetail *declContact = new QDeclarativeContactDetail(this);
            declContact->setContactDetail(sourceContact);
            declContacts.append(QVariant::fromValue(qobject_cast<QObject *>(declContact)));
        }
        m_contactDetails->insert(contactType, declContacts);
    }
    primarySignalsEmission();
}

/*!
    \internal
    Helper function to emit the signals for the primary___()
    fields.  It is expected that the values of the primary___()
    functions have already been modified to new values.
*/
void QDeclarativePlace::primarySignalsEmission(const QString &type)
{
    if (type.isEmpty() || type == QPlaceContactDetail::Phone) {
        if (m_prevPrimaryPhone != primaryPhone()) {
            m_prevPrimaryPhone = primaryPhone();
            emit primaryPhoneChanged();
        }
        if (!type.isEmpty())
            return;
    }

    if (type.isEmpty() || type == QPlaceContactDetail::Email) {
        if (m_prevPrimaryEmail != primaryEmail()) {
            m_prevPrimaryEmail = primaryEmail();
            emit primaryEmailChanged();
        }
        if (!type.isEmpty())
            return;
    }

    if (type.isEmpty() || type == QPlaceContactDetail::Website) {
        if (m_prevPrimaryWebsite != primaryWebsite()) {
            m_prevPrimaryWebsite = primaryWebsite();
            emit primaryWebsiteChanged();
        }
        if (!type.isEmpty())
            return;
    }

    if (type.isEmpty() || type == QPlaceContactDetail::Fax) {
        if (m_prevPrimaryFax != primaryFax()) {
            m_prevPrimaryFax = primaryFax();
            emit primaryFaxChanged();
        }
    }
}

/*!
    \internal
    Helper function to return the manager, this manager is intended to be used
    to perform the next operation.  If a an operation is currently underway
    then return a null pointer.
*/
QPlaceManager *QDeclarativePlace::manager()
{
    if (m_status != QDeclarativePlace::Ready && m_status != QDeclarativePlace::Error)
        return 0;

    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = 0;
    }

    if (!m_plugin) {
           qmlInfo(this) << QStringLiteral("Plugin is not assigned to place.");
           return 0;
    }

    QGeoServiceProvider *serviceProvider = m_plugin->sharedGeoServiceProvider();
    if (!serviceProvider)
        return 0;

    QPlaceManager *placeManager = serviceProvider->placeManager();

    if (!placeManager) {
        setStatus(Error, QCoreApplication::translate(CONTEXT_NAME, PLUGIN_ERROR)
                         .arg(m_plugin->name()).arg(serviceProvider->errorString()));
        return 0;
    }

    return placeManager;
}

/*!
    \internal
*/
QString QDeclarativePlace::primaryValue(const QString &contactType) const
{
    QVariant value = m_contactDetails->value(contactType);
    if (value.userType() == qMetaTypeId<QJSValue>())
        value = value.value<QJSValue>().toVariant();

    if (value.userType() == QVariant::List) {
        QVariantList detailList = m_contactDetails->value(contactType).toList();
        if (!detailList.isEmpty()) {
            QDeclarativeContactDetail *primaryDetail = qobject_cast<QDeclarativeContactDetail *>(detailList.at(0).value<QObject *>());
            if (primaryDetail)
                return primaryDetail->value();
        }
    } else if (value.userType() == QMetaType::QObjectStar) {
        QDeclarativeContactDetail *primaryDetail = qobject_cast<QDeclarativeContactDetail *>(m_contactDetails->value(contactType).value<QObject *>());
        if (primaryDetail)
            return primaryDetail->value();
    }

    return QString();
}
