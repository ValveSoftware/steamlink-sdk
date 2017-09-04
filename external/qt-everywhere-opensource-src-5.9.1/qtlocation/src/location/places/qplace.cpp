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

#include "qplace.h"
#include "qplace_p.h"

#ifdef QPLACE_DEBUG
#include <QDebug>
#endif

#include <QStringList>

QT_BEGIN_NAMESPACE

/*!
    \class QPlace
    \inmodule QtLocation
    \ingroup QtLocation-places
    \ingroup QtLocation-places-data
    \since 5.6

    \brief The QPlace class represents a set of data about a place.

    \input place-definition.qdocinc

    \section2 Contact Information
    The contact information of a place is based around a common set of
    \l {Contact Types}{contact types}. To retrieve all the phone numbers
    of a place, one would do:

    \snippet places/requesthandler.h Phone numbers

    The contact types are string values by design to allow for providers
    to introduce new contact types.

    For convenience there are a set of functions which return the value
    of the first contact detail of each type.
    \list
        \li QPlace::primaryPhone()
        \li QPlace::primaryEmail()
        \li QPlace::primaryWebsite()
        \li QPlace::primaryFax()
    \endlist

    \section2 Extended Attributes
    Places may have additional attributes which are not covered in the formal API.
    Similar to contacts attributes are based around a common set of
    \l {Attribute Types}{attribute types}.  To retrieve an extended attribute one
    would do:
    \snippet places/requesthandler.h Opening hours

    The attribute types are string values by design to allow providers
    to introduce new attribute types.

    \section2 Content
    The QPlace object is only meant to be a convenient container to hold
    rich content such as images, reviews and so on.  Retrieval of content
    should happen via QPlaceManager::getPlaceContent().

    The content is stored as a QPlaceContent::Collection which contains
    both the index of the content, as well as the content itself.  This enables
    developers to check whether a particular item has already been retrieved
    and if not, then request that content.

    \section3 Attribution
    Places have a field for a rich text attribution string.  Some providers
    may require that the attribution be shown when a place is displayed
    to a user.

    \section2 Categories
    Different categories may be assigned to a place to indicate that the place
    is associated with those categories.  When saving a place, the only meaningful
    data is the category id, the rest of the category data is effectively ignored.
    The category must already exist before saving the place (it is not possible
    to create a new category, assign it to the place, save the place and expect
    the category to be created).

    \section2 Saving Caveats
    \input place-caveats.qdocinc
*/

/*!
    Constructs an empty place object.
*/
QPlace::QPlace()
        : d_ptr(new QPlacePrivate())
{
}

/*!
    Constructs a copy of \a other.
*/
QPlace::QPlace(const QPlace &other)
        : d_ptr(other.d_ptr)
{
}

/*!
    Destroys this place.
*/
QPlace::~QPlace()
{
}

/*!
    Assigns \a other to this place and returns a reference
    to this place.
*/
QPlace &QPlace::operator= (const QPlace & other)
{
    if (this == &other)
        return *this;

    d_ptr = other.d_ptr;
    return *this;
}

inline QPlacePrivate *QPlace::d_func()
{
    return static_cast<QPlacePrivate *>(d_ptr.data());
}

inline const QPlacePrivate *QPlace::d_func() const
{
    return static_cast<const QPlacePrivate *>(d_ptr.constData());
}

/*!
    Returns true if \a other is equal to this place,
    otherwise returns false.
*/
bool QPlace::operator== (const QPlace &other) const
{
    Q_D(const QPlace);
    return *d == *other.d_func();
}

/*!
    Returns true if \a other is not equal to this place,
    otherwise returns false.
*/
bool QPlace::operator!= (const QPlace &other) const
{
    Q_D(const QPlace);
    return !(*d == *other.d_func());
}

/*!
    Returns categories that this place belongs to.
*/
QList<QPlaceCategory> QPlace::categories() const
{
    Q_D(const QPlace);
    return d->categories;
}

/*!
    Sets a single \a category that this place belongs to.
*/
void QPlace::setCategory(const QPlaceCategory &category)
{
    Q_D(QPlace);
    d->categories.clear();
    d->categories.append(category);
}

/*!
    Sets the \a categories that this place belongs to.
*/
void QPlace::setCategories(const QList<QPlaceCategory> &categories)
{
    Q_D(QPlace);
    d->categories = categories;
}

/*!
    Returns the location of the place.
*/
QGeoLocation QPlace::location() const
{
    Q_D(const QPlace);
    return d->location;
}

/*!
    Sets the \a location of the place.
*/
void QPlace::setLocation(const QGeoLocation &location)
{
    Q_D(QPlace);
    d->location = location;
}

/*!
    Returns an aggregated rating of the place.
*/
QPlaceRatings QPlace::ratings() const
{
    Q_D(const QPlace);
    return d->ratings;
}

/*!
    Sets the aggregated \a rating of the place.
*/
void QPlace::setRatings(const QPlaceRatings &rating)
{
    Q_D(QPlace);
    d->ratings = rating;
}

/*!
    Returns the supplier of this place.
*/
QPlaceSupplier QPlace::supplier() const
{
    Q_D(const QPlace);
    return d->supplier;
}

/*!
    Sets the supplier of this place to \a supplier.
*/
void QPlace::setSupplier(const QPlaceSupplier &supplier)
{
    Q_D(QPlace);
    d->supplier = supplier;
}

/*!
    Returns a collection of content associated with a place.
    This collection is a map with the key being the index of the content object
    and value being the content object itself.

    The \a type specifies which kind of content is to be retrieved.
*/
QPlaceContent::Collection QPlace::content(QPlaceContent::Type type) const
{
    Q_D(const QPlace);
    return d->contentCollections.value(type);
}

/*!
    Sets a collection of \a content for the given \a type.
*/
void QPlace::setContent(QPlaceContent::Type type, const QPlaceContent::Collection &content)
{
    Q_D(QPlace);
    d->contentCollections.insert(type, content);
}

/*!
    Adds a collection of \a content of the given \a type to the place.  Any index in \a content
    that already exists is overwritten.
*/
void QPlace::insertContent(QPlaceContent::Type type, const QPlaceContent::Collection &content)
{
    Q_D(QPlace);
    QMapIterator<int, QPlaceContent> iter(content);
    while (iter.hasNext()) {
        iter.next();
        d->contentCollections[type].insert(iter.key(), iter.value());
    }
}

/*!
    Returns the total count of content objects of the given \a type.
    This total count indicates how many the manager/provider should have available.
    (As opposed to how many objects this place instance is currently assigned).

    A negative count indicates that the total number of items is unknown.
    By default the total content count is set to 0.
*/
int QPlace::totalContentCount(QPlaceContent::Type type) const
{
    Q_D(const QPlace);
    return d->contentCounts.value(type, 0);
}

/*!
    Sets the \a totalCount of content objects of the given \a type.
*/
void QPlace::setTotalContentCount(QPlaceContent::Type type, int totalCount)
{
    Q_D(QPlace);
    d->contentCounts.insert(type, totalCount);
}

/*!
    Returns the name of the place.
*/
QString QPlace::name() const
{
    Q_D(const QPlace);
    return d->name;
}

/*!
    Sets the \a name of the place.
*/
void QPlace::setName(const QString &name)
{
    Q_D(QPlace);
    d->name = name;
}

/*!
    Returns the identifier of the place.  The place identifier is only meaningful to the QPlaceManager that
    generated it and is not transferable between managers.  The place identifier is not guaranteed
    to be universally unique, but unique for the manager that generated it.
*/
QString QPlace::placeId() const
{
    Q_D(const QPlace);
    return d->placeId;
}

/*!
    Sets the \a identifier of the place.
*/
void QPlace::setPlaceId(const QString &identifier)
{
    Q_D(QPlace);
    d->placeId = identifier;
}

/*!
    Returns a rich text attribution string of the place.  Note, some providers may have a
    requirement where the attribution must be shown whenever a place is displayed to an end user.
*/
QString QPlace::attribution() const
{
    Q_D(const QPlace);
    return d->attribution;
}

/*!
    Sets the \a attribution string of the place.
*/
void QPlace::setAttribution(const QString &attribution)
{
    Q_D(QPlace);
    d->attribution = attribution;
}

/*!
    Returns the icon of the place.
*/
QPlaceIcon QPlace::icon() const
{
    Q_D(const QPlace);
    return d->icon;
}

/*!
    Sets the \a icon of the place.
*/
void QPlace::setIcon(const QPlaceIcon &icon)
{
    Q_D(QPlace);
    d->icon = icon;
}

/*!
    Returns the primary phone number for this place.  This accesses the first contact detail
    of the \l {QPlaceContactDetail::Phone}{phone number type}.  If no phone details exist, then an empty string is returned.
*/
QString QPlace::primaryPhone() const
{
    Q_D(const QPlace);
    QList<QPlaceContactDetail> phoneNumbers = d->contacts.value(QPlaceContactDetail::Phone);
    if (!phoneNumbers.isEmpty())
        return phoneNumbers.at(0).value();
    else
        return QString();
}

/*!
    Returns the primary fax number for this place.  This convenience function accesses the first contact
    detail of the \l {QPlaceContactDetail::Fax}{fax type}.  If no fax details exist, then an empty string is returned.
*/
QString QPlace::primaryFax() const
{
    Q_D(const QPlace);
    QList<QPlaceContactDetail> faxNumbers = d->contacts.value(QPlaceContactDetail::Fax);
    if (!faxNumbers.isEmpty())
        return faxNumbers.at(0).value();
    else
        return QString();
}

/*!
    Returns the primary email address for this place.  This convenience function accesses the first
    contact detail of the \l {QPlaceContactDetail::Email}{email type}.  If no email addresses exist, then
    an empty string is returned.
*/
QString QPlace::primaryEmail() const
{
    Q_D(const QPlace);
    QList<QPlaceContactDetail> emailAddresses = d->contacts.value(QPlaceContactDetail::Email);
    if (!emailAddresses.isEmpty())
        return emailAddresses.at(0).value();
    else
        return QString();
}

/*!
    Returns the primary website of the place.  This convenience function accesses the first
    contact detail of the \l {QPlaceContactDetail::Website}{website type}.  If no websites exist,
    then an empty string is returned.
*/
QUrl QPlace::primaryWebsite() const
{
    Q_D(const QPlace);
    QList<QPlaceContactDetail> websites = d->contacts.value(QPlaceContactDetail::Website);
    if (!websites.isEmpty())
        return QUrl(websites.at(0).value());
    else
        return QString();
}

/*!
    Returns true if the details of this place have been fetched,
    otherwise returns false.
*/
bool QPlace::detailsFetched() const
{
    Q_D(const QPlace);
    return d->detailsFetched;
}

/*!
    Sets whether the details of this place have been \a fetched or not.
*/
void QPlace::setDetailsFetched(bool fetched)
{
    Q_D(QPlace);
    d->detailsFetched = fetched;
}

/*!
    Returns the types of extended attributes that this place has.
*/
QStringList QPlace::extendedAttributeTypes() const
{
    Q_D(const QPlace);
    return d->extendedAttributes.keys();
}

/*!
    Returns the exteded attribute corresponding to the specified \a attributeType.
    If the place does not have that particular attribute type, a default constructed
    QPlaceExtendedAttribute is returned.
*/
QPlaceAttribute QPlace::extendedAttribute(const QString &attributeType) const
{
    Q_D(const QPlace);
    return d->extendedAttributes.value(attributeType);
}

/*!
    Assigns an \a attribute of the given \a attributeType to a place.  If the given \a attributeType
    already exists in the place, then it is overwritten.

    If \a attribute is a default constructed QPlaceAttribute, then the \a attributeType
    is removed from the place which means it will not be listed by QPlace::extendedAttributeTypes().
*/
void QPlace::setExtendedAttribute(const QString &attributeType,
                                    const QPlaceAttribute &attribute)
{
    Q_D(QPlace);
    if (attribute == QPlaceAttribute())
        d->extendedAttributes.remove(attributeType);
    else
        d->extendedAttributes.insert(attributeType, attribute);
}

/*!
    Remove the attribute of \a attributeType from the place.

    The attribute will no longer be listed by QPlace::extendedAttributeTypes()
*/
void QPlace::removeExtendedAttribute(const QString &attributeType)
{
    Q_D(QPlace);
    d->extendedAttributes.remove(attributeType);
}

/*!
    Returns the type of contact details this place has.

    See QPlaceContactDetail for a list of common \l {QPlaceContactDetail::Email}{contact types}.
*/
QStringList QPlace::contactTypes() const
{
    Q_D(const QPlace);
    return d->contacts.keys();
}

/*!
    Returns a list of contact details of the specified \a contactType.

    See QPlaceContactDetail for a list of common \l {QPlaceContactDetail::Email}{contact types}.
*/
QList<QPlaceContactDetail> QPlace::contactDetails(const QString &contactType) const
{
    Q_D(const QPlace);
    return d->contacts.value(contactType);
}

/*!
    Sets the contact \a details of a specified \a contactType.

    If \a details is empty, then the \a contactType is removed from the place such
    that it is no longer returned by QPlace::contactTypes().

    See QPlaceContactDetail for a list of common \l {QPlaceContactDetail::Email}{contact types}.
*/
void QPlace::setContactDetails(const QString &contactType, QList<QPlaceContactDetail> details)
{
    Q_D(QPlace);
    if (details.isEmpty())
        d->contacts.remove(contactType);
    else
        d->contacts.insert(contactType, details);
}

/*!
    Appends a contact \a detail of a specified \a contactType.

    See QPlaceContactDetail for a list of common \l {QPlaceContactDetail::Email}{contact types}.
*/
void QPlace::appendContactDetail(const QString &contactType, const QPlaceContactDetail &detail)
{
    Q_D(QPlace);
    QList<QPlaceContactDetail> details = d->contacts.value(contactType);
    details.append(detail);
    d->contacts.insert(contactType, details);
}

/*!
    Removes all the contact details of a given \a contactType.

    The \a contactType is no longer returned when QPlace::contactTypes() is called.
*/
void QPlace::removeContactDetails(const QString &contactType)
{
    Q_D(QPlace);
    d->contacts.remove(contactType);
}

/*!
    Sets the visibility of the place to \a visibility.
*/
void QPlace::setVisibility(QLocation::Visibility visibility)
{
    Q_D(QPlace);
    d->visibility = visibility;
}

/*!
    Returns the visibility of the place.

    The default visibility of a new place is set to QtLocatin::Unspecified visibility.
    If a place is saved with unspecified visibility the backend chooses an appropriate
    default visibility to use when saving.
*/
QLocation::Visibility QPlace::visibility() const
{
    Q_D(const QPlace);
    return d->visibility;
}

/*!
    Returns a boolean indicating whether the all the fields of the place are empty or not.
*/
bool QPlace::isEmpty() const
{
    Q_D(const QPlace);
    return d->isEmpty();
}

/*******************************************************************************
*******************************************************************************/

QPlacePrivate::QPlacePrivate()
:   QSharedData(), visibility(QLocation::UnspecifiedVisibility), detailsFetched(false)
{
}

QPlacePrivate::QPlacePrivate(const QPlacePrivate &other)
        : QSharedData(other),
        categories(other.categories),
        location(other.location),
        ratings(other.ratings),
        supplier(other.supplier),
        name(other.name),
        placeId(other.placeId),
        attribution(other.attribution),
        contentCollections(other.contentCollections),
        contentCounts(other.contentCounts),
        extendedAttributes(other.extendedAttributes),
        contacts(other.contacts),
        visibility(other.visibility),
        icon(other.icon),
        detailsFetched(other.detailsFetched)
{
}

QPlacePrivate::~QPlacePrivate() {}

QPlacePrivate &QPlacePrivate::operator= (const QPlacePrivate & other)
{
    if (this == &other)
        return *this;

    categories = other.categories;
    location = other.location;
    ratings = other.ratings;
    supplier = other.supplier;
    name = other.name;
    placeId = other.placeId;
    attribution = other.attribution;
    contentCollections = other.contentCollections;
    contentCounts = other.contentCounts;
    contacts = other.contacts;
    extendedAttributes = other.extendedAttributes;
    visibility = other.visibility;
    detailsFetched = other.detailsFetched;
    icon = other.icon;

    return *this;
}

bool QPlacePrivate::operator== (const QPlacePrivate &other) const
{
#ifdef QPLACE_DEBUG
    qDebug() << "categories: " << (categories == other.categories);
    qDebug() << "location:" << (location == other.location);
    qDebug() << "ratings" << (ratings == other.ratings);
    qDebug() << "supplier" << (supplier == other.supplier);
    qDebug() << "contentCollections " << (contentCollections == other.contentCollections);
    qDebug() << "contentCounts " << (contentCounts == other.contentCounts);
    qDebug() << "name " << (name == other.name);
    qDebug() << "placeId" << (placeId == other.placeId);
    qDebug() << "attribution" << (attribution == other.attribution);
    qDebug() << "contacts" << (contacts == other.contacts);
    qDebug() << "extendedAttributes" << (extendedAttributes == other.extendedAttributes);
    qDebug() << "visibility" << (visibility == other.visibility);
    qDebug() << "icon" << (icon == other.icon);
#endif

    return (categories == other.categories
            && location == other.location
            && ratings == other.ratings
            && supplier == other.supplier
            && contentCollections == other.contentCollections
            && contentCounts == other.contentCounts
            && name == other.name
            && placeId == other.placeId
            && attribution == other.attribution
            && contacts == other.contacts
            && extendedAttributes == other.extendedAttributes
            && visibility == other.visibility
            && icon == other.icon
            );
}


bool QPlacePrivate::isEmpty() const
{
    return (categories.isEmpty()
            && location.isEmpty()
            && ratings.isEmpty()
            && supplier.isEmpty()
            && contentCollections.isEmpty()
            && contentCounts.isEmpty()
            && name.isEmpty()
            && placeId.isEmpty()
            && attribution.isEmpty()
            && contacts.isEmpty()
            && extendedAttributes.isEmpty()
            && QLocation::UnspecifiedVisibility == visibility
            && icon.isEmpty()
            );
}

QT_END_NAMESPACE
