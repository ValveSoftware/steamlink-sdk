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

#include "qplacereview.h"
#include "qplacereview_p.h"

QT_BEGIN_NAMESPACE

QPlaceReviewPrivate::QPlaceReviewPrivate()
:   QPlaceContentPrivate(), rating(0)
{
}

QPlaceReviewPrivate::QPlaceReviewPrivate(const QPlaceReviewPrivate &other)
    : QPlaceContentPrivate(other)
{
    dateTime = other.dateTime;
    text = other.text;
    language = other.language;
    rating = other.rating;
    reviewId = other.reviewId;
    title = other.title;
}

QPlaceReviewPrivate::~QPlaceReviewPrivate()
{
}

bool QPlaceReviewPrivate::compare(const QPlaceContentPrivate *other) const
{
    const QPlaceReviewPrivate *od = static_cast<const QPlaceReviewPrivate *>(other);
    return QPlaceContentPrivate::compare(other) &&
           dateTime == od->dateTime &&
           text == od->text &&
           language == od->language &&
           rating == od->rating &&
           reviewId == od->reviewId &&
           title == od->title;
}

/*!
    \class QPlaceReview
    \inmodule QtLocation
    \ingroup QtLocation-places
    \ingroup QtLocation-places-data
    \since 5.6

    \brief The QPlaceReview class represents a review of a place.

    Each QPlaceReview has a number of properties such as
    a title, text, date of submission and rating; in addition to those properties
    inherited from QPlaceContent.

    Note: The Places API only supports reviews as 'retrieve-only' objects.  Submitting reviews
    to a provider is not a supported use case.

    \sa QPlaceContent, QPlaceEditorial
*/

/*!
    Constructs a new review object.
*/
QPlaceReview::QPlaceReview()
    : QPlaceContent(new QPlaceReviewPrivate)
{
}

/*!
    \fn QPlaceReview::QPlaceReview(const QPlaceContent &other)
    Constructs a copy of \a other, otherwise constructs a default review object.
*/
Q_IMPLEMENT_CONTENT_COPY_CTOR(QPlaceReview)


/*!
    Destroys the review.
*/
QPlaceReview::~QPlaceReview()
{
}

Q_IMPLEMENT_CONTENT_D_FUNC(QPlaceReview)

/*!
    Returns the date and time that the review was submitted.
*/
QDateTime QPlaceReview::dateTime() const
{
    Q_D(const QPlaceReview);
    return d->dateTime;
}

/*!
    Sets the date and time that the review was submitted to \a dateTime.
*/
void QPlaceReview::setDateTime(const QDateTime &dateTime)
{
    Q_D(QPlaceReview);
    d->dateTime = dateTime;
}

/*!
    Returns a textual description of the place.

    Depending on the provider the text could be rich (HTML based) or plain text.
*/
QString QPlaceReview::text() const
{
    Q_D(const QPlaceReview);
    return d->text;
}

/*!
    Sets \a text of the review.
*/
void QPlaceReview::setText(const QString &text)
{
    Q_D(QPlaceReview);
    d->text = text;
}

/*!
    Returns the language of the review.  Typically this would be a language code
    in the 2 letter ISO 639-1 format.
*/
QString QPlaceReview::language() const
{
    Q_D(const QPlaceReview);
    return d->language;
}

/*!
    Sets the \a language of the review.  Typically this would be a language code
    in the 2 letter ISO 639-1 format.
*/
void QPlaceReview::setLanguage(const QString &language)
{
    Q_D(QPlaceReview);
    d->language = language;
}

/*!
    Returns this review's rating of the place.
*/
qreal QPlaceReview::rating() const
{
    Q_D(const QPlaceReview);
    return d->rating;
}

/*!
    Sets the review's \a rating of the place.
*/
void QPlaceReview::setRating(qreal rating)
{
    Q_D(QPlaceReview);
    d->rating = rating;
}

/*!
    Returns the review's identifier.
*/
QString QPlaceReview::reviewId() const
{
    Q_D(const QPlaceReview);
    return d->reviewId;
}

/*!
    Sets the \a identifier of the review.
*/
void QPlaceReview::setReviewId(const QString &identifier)
{
    Q_D(QPlaceReview);
    d->reviewId = identifier;
}

/*!
    Returns the title of the review.
*/
QString QPlaceReview::title() const
{
    Q_D(const QPlaceReview);
    return d->title;
}

/*!
    Sets the \a title of the review.
*/
void QPlaceReview::setTitle(const QString &title)
{
    Q_D(QPlaceReview);
    d->title = title;
}

QT_END_NAMESPACE
