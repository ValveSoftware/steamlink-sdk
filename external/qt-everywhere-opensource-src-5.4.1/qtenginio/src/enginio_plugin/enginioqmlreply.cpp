/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtEnginio module of the Qt Toolkit.
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

#include "enginioqmlreply_p.h"
#include "enginioqmlclient_p_p.h"
#include "enginioqmlclient_p.h"
#include <Enginio/private/enginioreply_p.h>
#include <QtQml/qjsvalue.h>
#include <QtQml/qqmlengine.h>
#include <QtCore/qobject.h>
#include <QtCore/qmetaobject.h>

QT_BEGIN_NAMESPACE

/*!
  \qmltype EnginioReply
  \since 5.3
  \instantiates EnginioQmlReply
  \inqmlmodule Enginio
  \ingroup engino-qml
  \target EnginioModelQml

  \brief A reply to any Enginio request.

  The reply, when finished, contains information received from the server:
  \list
  \li Data - object, which is a result from an earlier request,
    see the \l {EnginioReply::data}{data} function
  \li Network status - in case of a network problem, additional information can
  be accessed through: errorType, errorString, networkError
  \li Backend status - a finished request is always associated with a backend status
  code, which is just an HTTP code, and it can be queried through backendStatus
  \endlist
*/

/*!
  \qmlproperty QJSValue EnginioReply::data
  The data of this reply.
*/

/*!
  \qmlproperty Enginio::ErrorType EnginioReply::errorType
  The type of error (if any) of this reply.
*/

/*!
  \qmlproperty QNetworkReply::NetworkError EnginioReply::networkError
  The network error (if any).
*/

/*!
  \qmlproperty string EnginioReply::errorString
  The error message if this reply was an error.
*/

/*!
  \qmlproperty int EnginioReply::backendStatus
  The backend status code.
*/

/*!
  \qmlproperty bool EnginioReply::isFinished
  The property holds \c true if this reply is finished, \c false otherwise.
*/

/*!
  \qmlproperty bool EnginioReply::isError
  The property holds \c true if this reply has finished with an error, \c false otherwise.
*/

class EnginioQmlReplyPrivate : public EnginioReplyStatePrivate
{
    Q_DECLARE_PUBLIC(EnginioQmlReply)
public:
    EnginioQmlReplyPrivate(EnginioQmlClientPrivate *client, QNetworkReply *reply)
        : EnginioReplyStatePrivate(client, reply)
    {
        Q_ASSERT(client);
    }

    void emitFinished() Q_DECL_OVERRIDE
    {
        Q_Q(EnginioQmlReply);
        q->setParent(0);
        QQmlEngine::setObjectOwnership(q, QQmlEngine::JavaScriptOwnership);
        emit q->finished(static_cast<EnginioQmlClientPrivate*>(_client)->jsengine()->newQObject(q));
    }

    QJSValue data() const
    {
        if (_data.isEmpty() && _nreply->isFinished())
            _data = _nreply->readAll();

        return static_cast<EnginioQmlClientPrivate*>(_client)->fromJson(_data);
    }
};


EnginioQmlReply::EnginioQmlReply(EnginioQmlClientPrivate *parent, QNetworkReply *reply)
    : EnginioReplyState(parent, reply, new EnginioQmlReplyPrivate(parent, reply))
{}

EnginioQmlReply::~EnginioQmlReply()
{}

QJSValue EnginioQmlReply::data() const
{
    Q_D(const EnginioQmlReply);
    return d->data();
}

QT_END_NAMESPACE
