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

#include "qplacesearchrequest.h"
#include "qgeocoordinate.h"
#include "qgeoshape.h"

#include <QtCore/QSharedData>
#include <QtCore/QList>
#include <QtCore/QVariant>

QT_BEGIN_NAMESPACE

class QPlaceSearchRequestPrivate : public QSharedData
{
public:
    QPlaceSearchRequestPrivate();
    QPlaceSearchRequestPrivate(const QPlaceSearchRequestPrivate &other);
    ~QPlaceSearchRequestPrivate();

    QPlaceSearchRequestPrivate &operator=(const QPlaceSearchRequestPrivate &other);
    bool operator==(const QPlaceSearchRequestPrivate &other) const;

    void clear();

    QString searchTerm;
    QList<QPlaceCategory> categories;
    QGeoShape searchArea;
    QString recommendationId;
    QLocation::VisibilityScope visibilityScope;
    QPlaceSearchRequest::RelevanceHint relevanceHint;
    int limit;
    QVariant searchContext;
};

QPlaceSearchRequestPrivate::QPlaceSearchRequestPrivate()
:   QSharedData(),
    visibilityScope(QLocation::UnspecifiedVisibility),
    relevanceHint(QPlaceSearchRequest::UnspecifiedHint),
    limit(-1)
{
}

QPlaceSearchRequestPrivate::QPlaceSearchRequestPrivate(const QPlaceSearchRequestPrivate &other)
    : QSharedData(other),
      searchTerm(other.searchTerm),
      categories(other.categories),
      searchArea(other.searchArea),
      recommendationId(other.recommendationId),
      visibilityScope(other.visibilityScope),
      relevanceHint(other.relevanceHint),
      limit(other.limit),
      searchContext(other.searchContext)
{
}

QPlaceSearchRequestPrivate::~QPlaceSearchRequestPrivate()
{
}

QPlaceSearchRequestPrivate &QPlaceSearchRequestPrivate::operator=(const QPlaceSearchRequestPrivate &other)
{
    if (this != &other) {
        searchTerm = other.searchTerm;
        categories = other.categories;
        searchArea = other.searchArea;
        recommendationId = other.recommendationId;
        visibilityScope = other.visibilityScope;
        relevanceHint = other.relevanceHint;
        limit = other.limit;
        searchContext = other.searchContext;
    }

    return *this;
}

bool QPlaceSearchRequestPrivate::operator==(const QPlaceSearchRequestPrivate &other) const
{
    return searchTerm == other.searchTerm &&
           categories == other.categories &&
           searchArea == other.searchArea &&
           recommendationId == other.recommendationId &&
           visibilityScope == other.visibilityScope &&
           relevanceHint == other.relevanceHint &&
           limit == other.limit &&
           searchContext == other.searchContext;
}

void QPlaceSearchRequestPrivate::clear()
{
    limit = -1;
    searchTerm.clear();
    categories.clear();
    searchArea = QGeoShape();
    recommendationId.clear();
    visibilityScope = QLocation::UnspecifiedVisibility;
    relevanceHint = QPlaceSearchRequest::UnspecifiedHint;
    searchContext.clear();
}

/*!
    \class QPlaceSearchRequest
    \inmodule QtLocation
    \ingroup QtLocation-places
    \ingroup QtLocation-places-requests
    \since 5.6

    \brief The QPlaceSearchRequest class represents the set of parameters for a search request.

    A typical search request may look like the following:
    \snippet places/requesthandler.h Search request

    Note that specifying a search center can be done by setting a circular search area that has
    a center but no radius.    The default radius is set to -1, which indicates an undefined radius.  The provider will
    interpret this as being free to choose its own default radius.

    The QPlaceSearchRequest is primarily used with the QPlaceManager to
    \l {QPlaceManager::search()} {search for places}, however it is also
    used to provide parameters for \l {QPlaceManager::searchSuggestions()}{generating search term suggestions}.
    Note that in this context only some of the parameters may be relevant. For example, the search area
    is useful in narrowing down relevant search suggestions, while other parameters such as relevance hint
    are not so applicable.

    Also be aware that providers may vary by which parameters they support for example some providers may not support
    paging while others do, some providers may honor relevance hints while others may completely ignore them,
    see the \l {Qt Location#Plugin References and Parameters}{plugin documentation} for more
    details.
*/

/*!
    \enum QPlaceSearchRequest::RelevanceHint

    Defines hints to help rank place results.
    \value UnspecifiedHint
        No explicit hint has been specified.
    \value DistanceHint
        Distance to a search center is relevant for the user.  Closer places
        are more highly weighted.  This hint is only useful
        if a circular search area is used in the query.
    \value LexicalPlaceNameHint
        Alphabetic ordering of places according to name is relevant to the user.
*/

/*!
    Default constructor. Constructs an new request object.
*/
QPlaceSearchRequest::QPlaceSearchRequest()
    : d_ptr(new QPlaceSearchRequestPrivate())
{
}

/*!
    Constructs a copy of \a other.
*/
QPlaceSearchRequest::QPlaceSearchRequest(const QPlaceSearchRequest &other)
    : d_ptr(other.d_ptr)
{
}

/*!
    Destroys the request object.
*/
QPlaceSearchRequest::~QPlaceSearchRequest()
{
}

/*!
    Assigns \a other to this search request and returns a reference
    to this search request.
*/
QPlaceSearchRequest &QPlaceSearchRequest::operator= (const QPlaceSearchRequest & other)
{
    if (this == &other)
        return *this;

    d_ptr = other.d_ptr;
    return *this;
}

/*!
    Returns true if \a other is equal to this search request,
    otherwise returns false.
*/
bool QPlaceSearchRequest::operator== (const QPlaceSearchRequest &other) const
{
    Q_D(const QPlaceSearchRequest);
    return *d == *other.d_func();
}

/*!
    Returns true if \a other is not equal to this search request,
    otherwise returns false.
*/
bool QPlaceSearchRequest::operator!= (const QPlaceSearchRequest &other) const
{
    Q_D(const QPlaceSearchRequest);
    return !(*d == *other.d_func());
}

/*!
    Returns the search term.
*/
QString QPlaceSearchRequest::searchTerm() const
{
    Q_D(const QPlaceSearchRequest);
    return d->searchTerm;
}

/*!
    Sets the search \a term.
*/
void QPlaceSearchRequest::setSearchTerm(const QString &term)
{
    Q_D(QPlaceSearchRequest);
    d->searchTerm = term;
}

/*!
    Return the categories to be used in the search request.
    Places need only to belong to one of the categories
    to be considered a match by the request.
*/
QList<QPlaceCategory> QPlaceSearchRequest::categories() const
{
    Q_D(const QPlaceSearchRequest);
    return d->categories;
}

/*!
    Sets the search request to search by a single \a category

    \sa setCategories()
*/
void QPlaceSearchRequest::setCategory(const QPlaceCategory &category)
{
    Q_D(QPlaceSearchRequest);
    d->categories.clear();

    if (!category.categoryId().isEmpty())
        d->categories.append(category);
}

/*!
    Sets the search request to search from the list of given \a categories.
    Any places returned during the search will match at least one of the \a
    categories.

    \sa setCategory()
*/
void QPlaceSearchRequest::setCategories(const QList<QPlaceCategory> &categories)
{
    Q_D(QPlaceSearchRequest);
    d->categories = categories;
}

/*!
    Returns the search area which will be used to limit search results.  The default search area is
    an invalid QGeoShape, indicating that no specific search area is defined.
*/
QGeoShape QPlaceSearchRequest::searchArea() const
{
    Q_D(const QPlaceSearchRequest);
    return d->searchArea;
}

/*!
    Sets the search request to search within the given \a area.
*/
void QPlaceSearchRequest::setSearchArea(const QGeoShape &area)
{
    Q_D(QPlaceSearchRequest);
    d->searchArea = area;
}

/*!
    Returns the place id which will be used to search for recommendations
    for similar places.
*/
QString QPlaceSearchRequest::recommendationId() const
{
    Q_D(const QPlaceSearchRequest);
    return d->recommendationId;
}

/*!
    Sets the \a placeId which will be used to search for recommendations.
*/
void QPlaceSearchRequest::setRecommendationId(const QString &placeId)
{
    Q_D(QPlaceSearchRequest);
    d->recommendationId = placeId;
}

/*!
    Returns backend specific additional search context associated with this place search request.
    The search context is typically set as part of a
    \l {QPlaceSearchResult::ProposedSearchResult}{proposed search results}.
*/
QVariant QPlaceSearchRequest::searchContext() const
{
    Q_D(const QPlaceSearchRequest);
    return d->searchContext;
}

/*!
    Sets the search context to \a context.

    \note This method is intended to be used by geo service plugins when returning search results
    of type \l QPlaceSearchResult::ProposedSearchResult.

    The search context is used by backends to store additional search context related to the search
    request. Other relevant fields should also be filled in. For example, if the search context
    encodes a text search the search term should also be set with \l setSearchTerm(). The search
    context allows additional search context to be kept which is not directly accessible via the
    Qt Location API.

    The search context can be of any type storable in a QVariant. The value of the search context
    is not intended to be use directly by applications.
*/
void QPlaceSearchRequest::setSearchContext(const QVariant &context)
{
    Q_D(QPlaceSearchRequest);
    d->searchContext = context;
}

/*!
    Returns the visibility scope used when searching for places.  The default value is
    QLocation::UnspecifiedVisibility meaning that no explicit scope has been assigned.
    Places of any scope may be returned during the search.
*/
QLocation::VisibilityScope QPlaceSearchRequest::visibilityScope() const
{
    Q_D(const QPlaceSearchRequest);
    return d->visibilityScope;
}

/*!
    Sets the visibility \a scope used when searching for places.
*/
void QPlaceSearchRequest::setVisibilityScope(QLocation::VisibilityScope scope)
{
    Q_D(QPlaceSearchRequest);
    d->visibilityScope = scope;
}

/*!
    Returns the relevance hint of the request.  The hint is given to the provider
    to help but not dictate the ranking of results. For example providing a distance hint
    may give closer places a higher ranking but it doesn't necessarily mean
    that he results will be ordered strictly according to distance.
*/
QPlaceSearchRequest::RelevanceHint QPlaceSearchRequest::relevanceHint() const
{
    Q_D(const QPlaceSearchRequest);
    return d->relevanceHint;
}

/*!
    Sets the relevance \a hint to be used when searching for a place.
*/
void QPlaceSearchRequest::setRelevanceHint(QPlaceSearchRequest::RelevanceHint hint)
{
    Q_D(QPlaceSearchRequest);
    d->relevanceHint = hint;
}

/*!
    Returns the maximum number of search results to retrieve.

    A negative value for limit means that it is undefined.  It is left up to the backend
    provider to choose an appropriate number of results to return.  The default limit is -1.
*/
int QPlaceSearchRequest::limit() const
{
    Q_D(const QPlaceSearchRequest);
    return d->limit;
}

/*!
    Set the maximum number of search results to retrieve to \a limit.
*/
void QPlaceSearchRequest::setLimit(int limit)
{
    Q_D(QPlaceSearchRequest);
    d->limit = limit;
}

/*!
    Clears the search request.
*/
void QPlaceSearchRequest::clear()
{
    Q_D(QPlaceSearchRequest);
    d->clear();
}

inline QPlaceSearchRequestPrivate *QPlaceSearchRequest::d_func()
{
    return static_cast<QPlaceSearchRequestPrivate *>(d_ptr.data());
}

inline const QPlaceSearchRequestPrivate *QPlaceSearchRequest::d_func() const
{
    return static_cast<const QPlaceSearchRequestPrivate *>(d_ptr.constData());
}

QT_END_NAMESPACE
