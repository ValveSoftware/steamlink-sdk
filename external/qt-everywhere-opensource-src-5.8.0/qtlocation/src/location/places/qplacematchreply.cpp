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

#include "qplacematchreply.h"
#include "qplacereply_p.h"


QT_BEGIN_NAMESPACE
class QPlaceMatchReplyPrivate : public QPlaceReplyPrivate
{
public:
    QPlaceMatchReplyPrivate(){}
    QList<QPlace> places;
    QPlaceMatchRequest request;
};

QT_END_NAMESPACE

QT_USE_NAMESPACE

/*!
    \class QPlaceMatchReply
    \inmodule QtLocation
    \ingroup QtLocation-places
    \ingroup QtLocation-places-replies
    \since 5.6

    \brief The QPlaceMatchReply class manages a place matching operation started by an
    instance of QPlaceManager.

    If the operation is successful, the number of places in the reply matches those
    in the request.  If a particular place in the request is not found, a default
    constructed place is used as a place holder in the reply. In this way, there
    is always a one is to one relationship between input places in the request,
    and output places in the reply.

    If the operation is not successful the number of places is always zero.

    See \l {Matching places between managers} for an example on how to use
    a match reply.

    \sa QPlaceMatchRequest, QPlaceManager
*/

/*!
    Constructs a match reply with a given \a parent.
*/
QPlaceMatchReply::QPlaceMatchReply(QObject *parent)
    : QPlaceReply(new QPlaceMatchReplyPrivate, parent)
{
}

/*!
    Destroys the match reply.
*/
QPlaceMatchReply::~QPlaceMatchReply()
{
}

/*!
    Returns the type of reply.
*/
QPlaceReply::Type QPlaceMatchReply::type() const
{
    return QPlaceReply::MatchReply;
}

 /*!
    Returns a list of matching places;
*/
QList<QPlace> QPlaceMatchReply::places() const
{
    Q_D(const QPlaceMatchReply);
    return d->places;
}

/*!
    Sets the list of matching \a places.
*/
void QPlaceMatchReply::setPlaces(const QList<QPlace> &places)
{
    Q_D(QPlaceMatchReply);
    d->places = places;
}

/*!
    Returns the match request that was used to generate this reply.
*/
QPlaceMatchRequest QPlaceMatchReply::request() const
{
    Q_D(const QPlaceMatchReply);
    return d->request;
}

/*!
    Sets the match \a request used to generate this reply.
*/
void QPlaceMatchReply::setRequest(const QPlaceMatchRequest &request)
{
    Q_D(QPlaceMatchReply);
    d->request = request;
}
