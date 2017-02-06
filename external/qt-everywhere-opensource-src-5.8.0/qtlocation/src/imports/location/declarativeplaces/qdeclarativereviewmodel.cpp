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

#include "qdeclarativereviewmodel_p.h"
#include "qdeclarativesupplier_p.h"

#include <QtCore/QDateTime>
#include <QtLocation/QPlaceReview>

QT_BEGIN_NAMESPACE

/*!
    \qmltype ReviewModel
    \instantiates QDeclarativeReviewModel
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-places
    \ingroup qml-QtLocation5-places-models
    \since Qt Location 5.5

    \brief Provides access to reviews of a \l Place.

    The ReviewModel is a read-only model used to fetch reviews about a \l Place.  The model
    incrementally fetches. The number of reviews which are fetched at a time is specified
    by the \l batchSize property. The total number of reviews available can be accessed via the
    \l totalCount property.

    To use the ReviewModel we need a view and a delegate. In this snippet we
    see the setting up of a ListView with a ReviewModel model and a delegate.

    \snippet places/views/ReviewView.qml  ReviewModel delegate

    The model returns data for the following roles:

    \table
        \header
            \li Role
            \li Type
            \li Description
        \row
            \li dateTime
            \li datetime
            \li The date and time that the review was posted.
        \row
            \li text
            \li string
            \li The review's textual description of the place.  It can be either rich (HTML based) text or plain text
               depending on the provider.
        \row
            \li language
            \li string
            \li The language that the review is written in.
        \row
            \li rating
            \li real
            \li The rating that the reviewer gave to the place.
        \row
            \li reviewId
            \li string
            \li The identifier of the review.
        \row
            \li title
            \li string
            \li The title of the review.
        \row
            \li supplier
            \li \l Supplier
            \li The supplier of the review.
        \row
            \li user
            \li \l {QtLocation::User}{User}
            \li The user who contributed the review.
        \row
            \li attribution
            \li string
            \li Attribution text which must be displayed when displaying the review.
    \endtable
*/

/*!
    \qmlproperty Place QtLocation::ReviewModel::place

    This property holds the Place that the reviews are for.
*/

/*!
    \qmlproperty int QtLocation::ReviewModel::batchSize

    This property holds the batch size to use when fetching more reviews.
*/

/*!
    \qmlproperty int QtLocation::ReviewModel::totalCount

    This property holds the total number of reviews for the place.
*/

QDeclarativeReviewModel::QDeclarativeReviewModel(QObject *parent)
:   QDeclarativePlaceContentModel(QPlaceContent::ReviewType, parent)
{
}

QDeclarativeReviewModel::~QDeclarativeReviewModel()
{
    qDeleteAll(m_suppliers);
}

/*!
    \internal
*/
QVariant QDeclarativeReviewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= rowCount(index.parent()) || index.row() < 0)
        return QVariant();

    const QPlaceReview &review = m_content.value(index.row());

    switch (role) {
    case DateTimeRole:
        return review.dateTime();
    case TextRole:
        return review.text();
    case LanguageRole:
        return review.language();
    case RatingRole:
        return review.rating();
    case ReviewIdRole:
        return review.reviewId();
    case TitleRole:
        return review.title();
    }

    return QDeclarativePlaceContentModel::data(index, role);
}

QHash<int, QByteArray> QDeclarativeReviewModel::roleNames() const
{
    QHash<int, QByteArray> roles = QDeclarativePlaceContentModel::roleNames();
    roles.insert(DateTimeRole, "dateTime");
    roles.insert(TextRole, "text");
    roles.insert(LanguageRole, "language");
    roles.insert(RatingRole, "rating");
    roles.insert(ReviewIdRole, "reviewId");
    roles.insert(TitleRole, "title");
    return roles;
}

QT_END_NAMESPACE
