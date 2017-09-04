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

#include "qplacematchrequest.h"

#include <QtCore/QSharedData>
#include <QtCore/QList>
#include <QtLocation/QPlaceResult>

QT_BEGIN_NAMESPACE

class QPlaceMatchRequestPrivate : public QSharedData
{
public:
    QPlaceMatchRequestPrivate();
    QPlaceMatchRequestPrivate(const QPlaceMatchRequestPrivate &other);
    ~QPlaceMatchRequestPrivate();

    QPlaceMatchRequestPrivate &operator=(const QPlaceMatchRequestPrivate &other);
    bool operator==(const QPlaceMatchRequestPrivate &other) const;

    void clear();

    QList<QPlace> places;
    QVariantMap parameters;
};

QPlaceMatchRequestPrivate::QPlaceMatchRequestPrivate()
    :   QSharedData()
{
}

QPlaceMatchRequestPrivate::QPlaceMatchRequestPrivate(const QPlaceMatchRequestPrivate &other)
    : QSharedData(other),
      places(other.places),
      parameters(other.parameters)
{
}

QPlaceMatchRequestPrivate::~QPlaceMatchRequestPrivate()
{
}

QPlaceMatchRequestPrivate &QPlaceMatchRequestPrivate::operator=(const QPlaceMatchRequestPrivate &other)
{
    if (this != &other) {
        places = other.places;
        parameters = other.parameters;
    }

    return *this;
}

bool QPlaceMatchRequestPrivate::operator==(const QPlaceMatchRequestPrivate &other) const
{
    return (places == other.places
            && parameters == other.parameters);
}

void QPlaceMatchRequestPrivate::clear()
{
    places.clear();
    parameters.clear();
}

/*!
    \class QPlaceMatchRequest
    \inmodule QtLocation
    \ingroup QtLocation-places
    \ingroup QtLocation-places-requests
    \since 5.6

    \brief The QPlaceMatchRequest class is used to find places from one manager that match those from another.  It represents
    a set of request parameters.

    Places from another manager that may have corresponding/matching places in the current manager are assigned using setPlaces() or setResults().
    A set of further parameters are specified which determines the criteria for matching.

    The typical key for matching is the QPlaceMatchRequest::AlternativeId, the value is an alternative identifier attribute type of the format
    x_id_<provider name> for example x_id_here.  The provider name is name supplied to the QGeoServiceProvider instance.

    See \l {Matching places between managers} for an example on how to use a match request.

    \sa QPlaceMatchReply, QPlaceManager
*/

/*!
   \variable QPlaceMatchRequest::AlternativeId
   The key to specify that matching is to be accomplished via an alternative place identifier.
*/
const QString QPlaceMatchRequest::AlternativeId(QLatin1String("alternativeId"));

/*!
    Default constructor. Constructs a new request object.
*/
QPlaceMatchRequest::QPlaceMatchRequest()
    : d_ptr(new QPlaceMatchRequestPrivate())
{
}

/*!
    Constructs a copy of \a other.
*/
QPlaceMatchRequest::QPlaceMatchRequest(const QPlaceMatchRequest &other)
    : d_ptr(other.d_ptr)
{
}

/*!
    Destroys the request object.
*/
QPlaceMatchRequest::~QPlaceMatchRequest()
{
}

/*!
    Assigns \a other to this search request and returns a reference
    to this match request.
*/
QPlaceMatchRequest &QPlaceMatchRequest::operator= (const QPlaceMatchRequest & other)
{
    if (this == &other)
        return *this;
    d_ptr = other.d_ptr;
    return *this;
}

/*!
    Returns true if \a other is equal to this match request,
    otherwise returns false.
*/
bool QPlaceMatchRequest::operator== (const QPlaceMatchRequest &other) const
{
    Q_D(const QPlaceMatchRequest);
    return *d == *other.d_func();
}

/*!
    Returns true if \a other is not equal to this match request,
    otherwise returns false.
*/
bool QPlaceMatchRequest::operator!= (const QPlaceMatchRequest &other) const
{
    Q_D(const QPlaceMatchRequest);
    return !(*d == *other.d_func());
}


/*!
    Returns a list of places which are to be matched.
*/
QList<QPlace> QPlaceMatchRequest::places() const
{
    Q_D(const QPlaceMatchRequest);
    return d->places;
}

/*!
    Sets a list of \a places which are to be matched.

    \sa setResults()
*/
void QPlaceMatchRequest::setPlaces(const QList<QPlace> places)
{
    Q_D(QPlaceMatchRequest);
    d->places = places;
}

/*!
    Convenience function which uses a set of search \a results to set
    the places which should be matched.

    \sa setPlaces()
*/
void QPlaceMatchRequest::setResults(const QList<QPlaceSearchResult> &results)
{
    Q_D(QPlaceMatchRequest);
    QList<QPlace> places;
    foreach (const QPlaceSearchResult &result, results) {
        if (result.type() == QPlaceSearchResult::PlaceResult) {
            QPlaceResult placeResult = result;
            places.append(placeResult.place());
        }
    }

    d->places = places;
}

/*!
    Returns the parameters for matching places.
*/
QVariantMap QPlaceMatchRequest::parameters() const
{
    Q_D(const QPlaceMatchRequest);
    return d->parameters;
}

/*!
    Sets the \a parameters for matching places.
*/
void QPlaceMatchRequest::setParameters(const QVariantMap &parameters)
{
    Q_D(QPlaceMatchRequest);
    d->parameters = parameters;
}

/*!
    Clears the match request.
*/
void QPlaceMatchRequest::clear()
{
    Q_D(QPlaceMatchRequest);
    d->clear();
}

inline QPlaceMatchRequestPrivate *QPlaceMatchRequest::d_func()
{
    return static_cast<QPlaceMatchRequestPrivate *>(d_ptr.data());
}

inline const QPlaceMatchRequestPrivate *QPlaceMatchRequest::d_func() const
{
    return static_cast<const QPlaceMatchRequestPrivate *>(d_ptr.constData());
}

QT_END_NAMESPACE
