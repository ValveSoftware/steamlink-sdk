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

#include "qplacecategory.h"
#include "qplacecategory_p.h"

QT_BEGIN_NAMESPACE

QPlaceCategoryPrivate::QPlaceCategoryPrivate()
:   visibility(QLocation::UnspecifiedVisibility)
{
}

QPlaceCategoryPrivate::QPlaceCategoryPrivate(const QPlaceCategoryPrivate &other)
:   QSharedData(other), categoryId(other.categoryId), name(other.name), visibility(other.visibility),
    icon(other.icon)
{
}

QPlaceCategoryPrivate::~QPlaceCategoryPrivate()
{
}

QPlaceCategoryPrivate &QPlaceCategoryPrivate::operator=(const QPlaceCategoryPrivate &other)
{
    if (this == &other)
        return *this;

    categoryId = other.categoryId;
    name = other.name;
    icon = other.icon;
    return *this;
}

bool QPlaceCategoryPrivate::isEmpty() const
{
    return  categoryId.isEmpty()
            && name.isEmpty()
            && icon.isEmpty()
            && QLocation::UnspecifiedVisibility == visibility;
}

/*!
    \class QPlaceCategory
    \inmodule QtLocation
    \ingroup QtLocation-places
    \ingroup QtLocation-places-data
    \since 5.6

    \brief The QPlaceCategory class represents a category that a \l QPlace can be associated with.

    Categories are used to search for places based on the categories they are associated with.  The
    list/tree of available categories can be obtained from \l QPlaceManager.  The
    \l QPlaceSearchRequest::setCategories() function can be used to limit the search results to
    places with the specified categories.

    If the \l QGeoServiceProvider supports it, categories can be created and removed.  This
    functionality is available in the \l QPlaceManager class.
*/

/*!
    \fn bool QPlaceCategory::operator!=(const QPlaceCategory &other) const

    Returns true if \a other is not equal to this category; otherwise returns false.
*/

/*!
    Constructs a category.
*/
QPlaceCategory::QPlaceCategory()
    : d(new QPlaceCategoryPrivate)
{
}

/*!
    Constructs a category which is a copy of \a other.
*/
QPlaceCategory::QPlaceCategory(const QPlaceCategory &other)
    :d(other.d)
{
}

/*!
    Destroys the category.
*/
QPlaceCategory::~QPlaceCategory()
{
}

/*!
    Assigns \a other to this category and returns a reference to this category.
*/
QPlaceCategory &QPlaceCategory::operator =(const QPlaceCategory &other)
{
    if (this == &other)
        return *this;

    d = other.d;
    return *this;
}

/*!
    Returns true if \a other is equal to this category; otherwise returns false.
*/
bool QPlaceCategory::operator==(const QPlaceCategory &other) const
{
    return d->categoryId == other.d->categoryId &&
           d->name == other.d->name &&
           d->visibility == other.d->visibility &&
           d->icon == other.d->icon;
}

/*!
    Returns the identifier of the category.  The category identifier is a string which uniquely identifies this category
    within a particular \l QPlaceManager.  The identifier is only meaningful to the QPlaceManager
    that generated it and is not transferable between managers.
*/
QString QPlaceCategory::categoryId() const
{
    return d->categoryId;
}

/*!
    Sets the \a identifier of the category.
*/
void QPlaceCategory::setCategoryId(const QString &identifier)
{
    d->categoryId = identifier;
}

/*!
    Returns the name of category.
*/
QString QPlaceCategory::name() const
{
    return d->name;
}

/*!
    Sets the \a name of the category.
*/
void QPlaceCategory::setName(const QString &name)
{
    d->name = name;
}

/*!
    Sets the \a visibility of the category.
*/
void QPlaceCategory::setVisibility(QLocation::Visibility visibility)
{
    d->visibility = visibility;
}

/*!
    Returns the visibility of the category.
*/
QLocation::Visibility QPlaceCategory::visibility() const
{
    return d->visibility;
}

/*!
    Returns the icon associated with the category.
*/
QPlaceIcon QPlaceCategory::icon() const
{
    return d->icon;
}

/*!
    Sets the \a icon of the category.
*/
void QPlaceCategory::setIcon(const QPlaceIcon &icon)
{
    d->icon = icon;
}

/*!
    Returns a boolean indicating whether the all the fields of the place category are empty or not.
*/
bool QPlaceCategory::isEmpty() const
{
    return d->isEmpty();
}

QT_END_NAMESPACE
