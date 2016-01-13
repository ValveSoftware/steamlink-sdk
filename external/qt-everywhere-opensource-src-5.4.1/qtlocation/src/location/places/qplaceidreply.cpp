/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
    \since Qt Location 5.0

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
