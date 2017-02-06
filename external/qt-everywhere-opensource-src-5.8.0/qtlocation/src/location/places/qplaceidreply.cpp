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

#include "qplaceidreply.h"
#include "qplacereply_p.h"

QT_BEGIN_NAMESPACE
class QPlaceIdReplyPrivate : public QPlaceReplyPrivate
{
public:
    QPlaceIdReplyPrivate(QPlaceIdReply::OperationType operationType)
        : operationType(operationType) {}
    ~QPlaceIdReplyPrivate() {}
    QString id;
    QPlaceIdReply::OperationType operationType;
};

QT_END_NAMESPACE

QT_USE_NAMESPACE

/*!
    \class QPlaceIdReply
    \inmodule QtLocation
    \ingroup QtLocation-places
    \ingroup QtLocation-places-replies
    \since 5.6

    \brief The QPlaceIdReply class manages operations which return an identifier such as
           saving and removal operations of places and categories.

    The QPlaceIdReply can be considered a multipurpose reply in that it can
    be used to save places, save categories, remove places and remove categories.
    In each case it returns an identifier of the place or category that was added, modified or removed.

    See \l {Saving a place cpp}{Saving a place} for an example of how to use an identifier reply.
    \sa QPlaceManager
*/

/*!
    \enum QPlaceIdReply::OperationType
    Defines the type of operation that was used to generate this reply.
    \value SavePlace The reply was created for a save place operation
    \value RemovePlace The reply was created for a remove place operation.
    \value SaveCategory The reply was created for a save category operation
    \value RemoveCategory The reply was created for a remove category operation.
*/

/*!
    Constructs a reply which contains the identifier of the object operated upon. The reply is for the given \a operationType and with \a parent.
*/
QPlaceIdReply::QPlaceIdReply(QPlaceIdReply::OperationType operationType, QObject *parent)
    : QPlaceReply(new QPlaceIdReplyPrivate(operationType), parent) {}

/*!
    Destroys the reply.
*/
QPlaceIdReply::~QPlaceIdReply()
{
}

/*!
    Returns the type of reply.
*/
QPlaceReply::Type QPlaceIdReply::type() const
{
    return QPlaceReply::IdReply;
}

/*!
    Returns the operation type of the reply. This means whether this
    identifier reply was for a save place operation,
    remove category operation and so on.
*/
QPlaceIdReply::OperationType QPlaceIdReply::operationType() const
{
    Q_D(const QPlaceIdReply);
    return d->operationType;
}

/*!
    Returns the relevant identifier for the operation. For example for a save place operation,
    the identifier is that of the saved place.  For a category removal operation,
    it is the identifier of the category that was removed.
*/
QString QPlaceIdReply::id() const
{
    Q_D(const QPlaceIdReply);
    return d->id;
}

/*!
    Sets the \a identifier of the reply.
*/
void QPlaceIdReply::setId(const QString &identifier)
{
    Q_D(QPlaceIdReply);
    d->id = identifier;
}
