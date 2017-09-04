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

#include "qplaceresult.h"
#include "qplaceresult_p.h"
#include <QtCore/qnumeric.h>

QT_USE_NAMESPACE

QPlaceResultPrivate::QPlaceResultPrivate()
    :   QPlaceSearchResultPrivate(), distance(qQNaN()), sponsored(false)
{
}

QPlaceResultPrivate::QPlaceResultPrivate(const QPlaceResultPrivate &other)
:   QPlaceSearchResultPrivate(other), distance(other.distance), place(other.place),
    sponsored(other.sponsored)
{
}

QPlaceResultPrivate::~QPlaceResultPrivate()
{
}

bool QPlaceResultPrivate::compare(const QPlaceSearchResultPrivate *other) const
{
    const QPlaceResultPrivate *od = static_cast<const QPlaceResultPrivate *>(other);
    return QPlaceSearchResultPrivate::compare(other)
           && ((qIsNaN(distance) && qIsNaN(od->distance))
                || qFuzzyCompare(distance, od->distance))
           && place == od->place
           && sponsored == od->sponsored;
}

/*!
    \class QPlaceResult
    \inmodule QtLocation
    \ingroup QtLocation-places
    \ingroup QtLocation-places-data
    \since 5.6

    \brief The QPlaceResult class represents a search result containing a place.

    The PlaceResult holds the distance to the place from the center of the search request,
    an instance of the place and an indication of whether the result is
    sponsored or \l {http://en.wikipedia.org/wiki/Organic_search}{organic}.

    The intended usage is that a QPlaceSearchResult can be converted into a QPlaceResult
    like so:

    \snippet places/requesthandler.h Convert search result

    The implementation is handled in such a way that object slicing is not an issue.

    \sa QPlaceSearchResult
*/

/*!
    Constructs a new place result object.
*/
QPlaceResult::QPlaceResult()
:   QPlaceSearchResult(new QPlaceResultPrivate)
{
}

/*!
    Destructor.
*/
QPlaceResult::~QPlaceResult()
{
}

/*!
    \fn QPlaceResult::QPlaceResult(const QPlaceSearchResult &other)
    Constructs a copy of \a other if possible, otherwise constructs a default place result.
*/
Q_IMPLEMENT_SEARCHRESULT_COPY_CTOR(QPlaceResult)

Q_IMPLEMENT_SEARCHRESULT_D_FUNC(QPlaceResult)

/*!
    Returns the distance of the place to the search center.  This
    field is only relevant provided the search request contained
    a search area with a search center.  Otherwise,
    the distance is NaN indicating an undefined distance.  The default value
    for distance is NaN.
*/
qreal QPlaceResult::distance() const
{
    Q_D(const QPlaceResult);
    return d->distance;
}

/*!
    Set the \a distance of the search result's place from a search center.
*/
void QPlaceResult::setDistance(qreal distance)
{
    Q_D(QPlaceResult);
    d->distance = distance;
}

/*!
    Returns the place of the search result.
*/
QPlace QPlaceResult::place() const
{
    Q_D(const QPlaceResult);
    return d->place;
}

/*!
    Sets the \a place that this result refers to.
*/
void QPlaceResult::setPlace(const QPlace &place)
{
    Q_D(QPlaceResult);
    d->place = place;
}

/*!
    Returns true if the result is a sponsored result.

    \sa setSponsored()
*/
bool QPlaceResult::isSponsored() const
{
    Q_D(const QPlaceResult);
    return d->sponsored;
}

/*!
    Sets whether the result is a \a sponsored result or not.

    \sa isSponsored()
*/
void QPlaceResult::setSponsored(bool sponsored)
{
    Q_D(QPlaceResult);
    d->sponsored = sponsored;
}
