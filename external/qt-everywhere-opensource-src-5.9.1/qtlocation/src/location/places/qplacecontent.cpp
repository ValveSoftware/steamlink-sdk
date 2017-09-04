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

#include "qplacecontent.h"
#include "qplacecontent_p.h"

#include <QtCore/QUrl>

QT_USE_NAMESPACE

template<> QPlaceContentPrivate *QSharedDataPointer<QPlaceContentPrivate>::clone()
{
    return d->clone();
}

inline QPlaceContentPrivate *QPlaceContent::d_func()
{
    return static_cast<QPlaceContentPrivate *>(d_ptr.data());
}

inline const QPlaceContentPrivate *QPlaceContent::d_func() const
{
    return static_cast<const QPlaceContentPrivate *>(d_ptr.constData());
}

bool QPlaceContentPrivate::compare(const QPlaceContentPrivate *other) const
{
    return supplier == other->supplier
            && user == other->user
            && attribution == other->attribution;
}

/*!
    \class QPlaceContent
    \inmodule QtLocation
    \ingroup QtLocation-places
    \ingroup QtLocation-places-data
    \since 5.6

    \brief The QPlaceContent class serves as the base class for rich content types.

    Rich content such as \l {QPlaceImage}{images}, \l {QPlaceReview}{reviews}
    and \l {QPlaceEditorial}{editorials} inherit
    from the QPlaceContent class which contains common properties such as
    an attribution string and content contributor, which may take the form of a
    \l {QPlaceUser}{user} and/or \l {QPlaceSupplier}{supplier}.  It is possible that
    a user from a supplier is contributing content, hence both fields could
    be filled in simultaneously.

    \b {Note:} Some providers may \e {require} that the attribution string be displayed
    to the user whenever a piece of content is viewed.

    Conversion between QPlaceContent and it's subclasses can be easily performed without
    casting. Due to the way it has been implemented, object slicing is not an issue,
    the following code is valid:
    \snippet places/requesthandler.h Content conversion

    The rich content of a place is typically made available as paginated items.  The ability
    to convert between QPlaceContent and it's subclasses means that code which handles
    the mechanics of paging can be easily shared for each of the sub types.

    At present the QPlaceContent class is not extensible by 3rd parties.

    Note:  The Places API considers content objects to be 'retrieve-only' objects.
    Submission of content to a provider is not a supported use case.
    \sa QPlaceImage, QPlaceReview, QPlaceEditorial
*/

/*!
    \typedef QPlaceContent::Collection
    Synonym for QMap<int, QPlaceContent>.  The key of the map is an \c int representing the
    index of the content.  The value is the content object itself.

    The \c {Collection} is intended to be a container where content items, that have been retrieved
    as pages, can be stored.  This enables a developer to skip pages, for example indexes 0-9 may be
    stored in the collection,  if the user skips to indexes 80-99, these can be stored in the
    collection as well.
*/

/*!
    \enum QPlaceContent::Type
    Defines the type of content.
    \value NoType
        The content object is default constructed, any other content type may be assigned
        to this content object.
    \value ImageType
        The content object is an image.
    \value ReviewType
        The content object is a review.
    \value EditorialType
        The content object is an editorial
*/

/*!
    Constructs an default content object which has no type.
*/
QPlaceContent::QPlaceContent()
    :d_ptr(0)
{
}

/*!
    Constructs a new copy of \a other.
*/
QPlaceContent::QPlaceContent(const QPlaceContent &other)
    :d_ptr(other.d_ptr)
{
}

/*!
    Assigns the \a other content object to this and returns a reference
    to this content object.
*/
QPlaceContent &QPlaceContent::operator=(const QPlaceContent &other)
{
    if (this == &other)
        return *this;

    d_ptr = other.d_ptr;
    return *this;
}

/*!
    Destroys the content object.
*/
QPlaceContent::~QPlaceContent()
{
}

/*!
    Returns the content type.
*/
QPlaceContent::Type QPlaceContent::type() const
{
    if (!d_ptr)
        return NoType;
    return d_ptr->type();
}

/*!
    Returns true if this content object is equivalent to \a other,
    otherwise returns false.
*/
bool QPlaceContent::operator==(const QPlaceContent &other) const
{
    // An invalid content object is only equal to another invalid content object
    if (!d_ptr)
        return !other.d_ptr;

    if (type() != other.type())
        return false;

    return d_ptr->compare(other.d_ptr);
}

/*!
    Returns true if this content object is not equivalent to \a other,
    otherwise returns false.
*/
bool QPlaceContent::operator!=(const QPlaceContent &other) const
{
    return !(*this == other);
}

/*!
    Returns the supplier who contributed this content.
*/
QPlaceSupplier QPlaceContent::supplier() const
{
    Q_D(const QPlaceContent);

    return d->supplier;
}

/*!
    Sets the \a supplier of the content.
*/
void QPlaceContent::setSupplier(const QPlaceSupplier &supplier)
{
    Q_D(QPlaceContent);

    d->supplier = supplier;
}

/*!
    Returns the user who contributed this content.
*/
QPlaceUser QPlaceContent::user() const
{
    Q_D(const QPlaceContent);
    return d->user;
}

/*!
    Sets the \a user who contributed this content.
*/
void QPlaceContent::setUser(const QPlaceUser &user)
{
    Q_D(QPlaceContent);
    d->user = user;
}

/*!
    Returns a rich text attribution string.

    \b {Note}: Some providers may require that the attribution
    of a particular content item always be displayed
    when the content item is shown.
*/
QString QPlaceContent::attribution() const
{
    Q_D(const QPlaceContent);
    return d->attribution;
}

/*!
    Sets a rich text \a attribution string for this content item.
*/
void QPlaceContent::setAttribution(const QString &attribution)
{
    Q_D(QPlaceContent);
    d->attribution = attribution;
}

/*!
    \internal
    Constructs a new content object from the given pointer \a d.
*/
QPlaceContent::QPlaceContent(QPlaceContentPrivate *d)
    :d_ptr(d)
{
}
