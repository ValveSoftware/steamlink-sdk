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

#include "qplacesupplier.h"
#include "qplacesupplier_p.h"

QT_USE_NAMESPACE

QPlaceSupplierPrivate::QPlaceSupplierPrivate() : QSharedData()
{
}

QPlaceSupplierPrivate::QPlaceSupplierPrivate(const QPlaceSupplierPrivate &other)
    : QSharedData()
{
    this->name = other.name;
    this->supplierId = other.supplierId;
    this->url = other.url;
    this->icon = other.icon;
}

QPlaceSupplierPrivate::~QPlaceSupplierPrivate()
{
}

bool QPlaceSupplierPrivate::operator==(const QPlaceSupplierPrivate &other) const
{
    return (
            this->name == other.name
            && this->supplierId == other.supplierId
            && this->url == other.url
            && this->icon == other.icon
    );
}

bool QPlaceSupplierPrivate::isEmpty() const
{
    return (name.isEmpty()
            && supplierId.isEmpty()
            && url.isEmpty()
            && icon.isEmpty()
            );
}

/*!
    \class QPlaceSupplier
    \inmodule QtLocation
    \ingroup QtLocation-places
    \ingroup QtLocation-places-data
    \since 5.6

    \brief The QPlaceSupplier class represents a supplier of a place or content associated
    with a place.

    Each instance represents a set of data about a supplier, which can include
    supplier's name, url and icon.  The supplier is typically a business or organization.

    Note: The Places API only supports suppliers as 'retrieve-only' objects.  Submitting
    suppliers to a provider is not a supported use case.
*/

/*!
    Constructs a new supplier object.
*/
QPlaceSupplier::QPlaceSupplier()
    : d(new QPlaceSupplierPrivate)
{
}

/*!
    Constructs a copy of \a other.
*/
QPlaceSupplier::QPlaceSupplier(const QPlaceSupplier &other)
    :d(other.d)
{
}

/*!
    Destroys the supplier object.
*/
QPlaceSupplier::~QPlaceSupplier()
{
}

/*!
    Assigns \a other to this supplier and returns a reference to this
    supplier.
*/
QPlaceSupplier &QPlaceSupplier::operator=(const QPlaceSupplier &other)
{
    if (this == &other)
        return *this;

    d = other.d;
    return *this;
}

/*!
    Returns true if this supplier is equal to \a other,
    otherwise returns false.
*/
bool QPlaceSupplier::operator==(const QPlaceSupplier &other) const
{
    return (*(d.constData()) == *(other.d.constData()));
}

/*!
    \fn QPlaceSupplier::operator!=(const QPlaceSupplier &other) const

    Returns true if this supplier is not equal to \a other,
    otherwise returns false.
*/

/*!
    Returns the name of the supplier which can be displayed to the user.

    The name can potentially be localized. The language is dependent on the
    entity that sets it, typically this is the QPlaceManager.
    The QPlaceManager::locales() field defines what language is used.
*/
QString QPlaceSupplier::name() const
{
    return d->name;
}

/*!
    Sets the \a name of the supplier.
*/
void QPlaceSupplier::setName(const QString &name)
{
    d->name = name;
}

/*!
    Returns the identifier of the supplier. The identifier is unique
    to the manager backend which provided the supplier and is generally
    not suitable for displaying to the user.
*/
QString QPlaceSupplier::supplierId() const
{
    return d->supplierId;
}

/*!
    Sets the \a identifier of the supplier.
*/
void QPlaceSupplier::setSupplierId(const QString &identifier)
{
    d->supplierId = identifier;
}

/*!
    Returns the URL of the supplier's website.
*/
QUrl QPlaceSupplier::url() const
{
    return d->url;
}

/*!
    Sets the \a url of the supplier's website.
*/
void QPlaceSupplier::setUrl(const QUrl &url)
{
    d->url = url;
}

/*!
    Returns the icon of the supplier.
*/
QPlaceIcon QPlaceSupplier::icon() const
{
    return d->icon;
}

/*!
    Sets the \a icon of the supplier.
*/
void QPlaceSupplier::setIcon(const QPlaceIcon &icon)
{
    d->icon = icon;
}

/*!
    Returns true if all fields of the place supplier are 0; otherwise returns false.
*/
bool QPlaceSupplier::isEmpty() const
{
    return d->isEmpty();
}
