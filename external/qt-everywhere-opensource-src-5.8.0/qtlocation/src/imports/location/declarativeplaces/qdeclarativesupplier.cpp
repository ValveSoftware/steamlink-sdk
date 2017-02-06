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

#include "qdeclarativesupplier_p.h"

#include <QtCore/QUrl>

QT_USE_NAMESPACE

/*!
    \qmltype Supplier
    \instantiates QDeclarativeSupplier
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-places
    \ingroup qml-QtLocation5-places-data
    \since Qt Location 5.5

    \brief Holds data regarding the supplier of a place, a place's image, review, or editorial.

    Each instance represents a set of data about a supplier, which can include
    supplier's name, url and icon.  The supplier is typically a business or organization.

    Note: The Places API only supports suppliers as 'retrieve-only' objects.  Submitting
    suppliers to a provider is not a supported use case.

    \sa ImageModel, ReviewModel, EditorialModel

    \section1 Example

    The following example shows how to create and display a supplier in QML:

    \snippet declarative/places.qml QtQuick import
    \snippet declarative/maps.qml QtLocation import
    \codeline
    \snippet declarative/places.qml Supplier
*/

QDeclarativeSupplier::QDeclarativeSupplier(QObject *parent)
    : QObject(parent), m_icon(0)
{
}

QDeclarativeSupplier::QDeclarativeSupplier(const QPlaceSupplier &src,
                                           QDeclarativeGeoServiceProvider *plugin,
                                           QObject *parent)
    : QObject(parent),
      m_src(src),
      m_icon(0)
{
    setSupplier(src, plugin);
}

QDeclarativeSupplier::~QDeclarativeSupplier()
{
}

/*!
    \internal
*/
void QDeclarativeSupplier::componentComplete()
{
    // delayed instantiation of QObject based properties.
    if (!m_icon)
        m_icon = new QDeclarativePlaceIcon(this);
}

/*!
    \qmlproperty QPlaceSupplier Supplier::supplier

    For details on how to use this property to interface between C++ and QML see
    "\l {Supplier - QPlaceSupplier} {Interfaces between C++ and QML Code}".
*/
void QDeclarativeSupplier::setSupplier(const QPlaceSupplier &src, QDeclarativeGeoServiceProvider *plugin)
{
    QPlaceSupplier previous = m_src;
    m_src = src;

   if (previous.name() != m_src.name())
        emit nameChanged();

    if (previous.supplierId() != m_src.supplierId())
        emit supplierIdChanged();

    if (previous.url() != m_src.url())
        emit urlChanged();

    if (m_icon && m_icon->parent() == this) {
        m_icon->setPlugin(plugin);
        m_icon->setIcon(m_src.icon());
    } else if (!m_icon || m_icon->parent() != this) {
        m_icon = new QDeclarativePlaceIcon(m_src.icon(), plugin, this);
        emit iconChanged();
    }
}

QPlaceSupplier QDeclarativeSupplier::supplier()
{
    m_src.setIcon(m_icon ? m_icon->icon() : QPlaceIcon());
    return m_src;
}

/*!
    \qmlproperty string Supplier::supplierId

    This property holds the identifier of the supplier.  The identifier is unique
    to the Plugin backend which provided the supplier and is generally
    not suitable for displaying to the user.
*/

void QDeclarativeSupplier::setSupplierId(const QString &supplierId)
{
    if (m_src.supplierId() != supplierId) {
        m_src.setSupplierId(supplierId);
        emit supplierIdChanged();
    }
}

QString QDeclarativeSupplier::supplierId() const
{
    return m_src.supplierId();
}

/*!
    \qmlproperty string Supplier::name

    This property holds the name of the supplier which can be displayed
    to the user.

    The name can potentially be localized.  The language is dependent on the
    entity that sets it, typically this is the \l Plugin.  The \l {Plugin::locales}
    property defines what language is used.
*/

void QDeclarativeSupplier::setName(const QString &name)
{
    if (m_src.name() != name) {
        m_src.setName(name);
        emit nameChanged();
    }
}

QString QDeclarativeSupplier::name() const
{
    return m_src.name();
}

/*!
    \qmlproperty url Supplier::url

    This property holds the URL of the supplier's website.
*/

void QDeclarativeSupplier::setUrl(const QUrl &url)
{
    if (m_src.url() != url) {
        m_src.setUrl(url);
        emit urlChanged();
    }
}

QUrl QDeclarativeSupplier::url() const
{
    return m_src.url();
}

/*!
    \qmlproperty PlaceIcon Supplier::icon

    This property holds the icon of the supplier.
*/
QDeclarativePlaceIcon *QDeclarativeSupplier::icon() const
{
    return m_icon;
}

void QDeclarativeSupplier::setIcon(QDeclarativePlaceIcon *icon)
{
    if (m_icon == icon)
        return;

    if (m_icon && m_icon->parent() == this)
        delete m_icon;

    m_icon = icon;
    emit iconChanged();
}
