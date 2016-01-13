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

#ifndef ENGINIOQMLCLIENT_H
#define ENGINIOQMLCLIENT_H

#include <Enginio/enginioclientconnection.h>
#include "enginioqmlreply_p.h"
#include <QtQml/qjsvalue.h>

QT_BEGIN_NAMESPACE

class EnginioQmlClientPrivate;
class EnginioQmlClient : public EnginioClientConnection
{
    Q_OBJECT
    Q_DISABLE_COPY(EnginioQmlClient)

    Q_ENUMS(Enginio::Operation); // TODO remove me QTBUG-33577
    Q_ENUMS(Enginio::AuthenticationState); // TODO remove me QTBUG-33577
public:
    EnginioQmlClient(QObject *parent = 0);
    ~EnginioQmlClient();

    Q_INVOKABLE EnginioQmlReply *fullTextSearch(const QJSValue &query);
    Q_INVOKABLE EnginioQmlReply *query(const QJSValue &query, const Enginio::Operation operation = Enginio::ObjectOperation);
    Q_INVOKABLE EnginioQmlReply *create(const QJSValue &object, const Enginio::Operation operation = Enginio::ObjectOperation);
    Q_INVOKABLE EnginioQmlReply *update(const QJSValue &object, const Enginio::Operation operation = Enginio::ObjectOperation);
    Q_INVOKABLE EnginioQmlReply *remove(const QJSValue &object, const Enginio::Operation operation = Enginio::ObjectOperation);
    Q_INVOKABLE EnginioQmlReply *downloadUrl(const QJSValue &object);
    Q_INVOKABLE EnginioQmlReply *uploadFile(const QJSValue &object, const QUrl &url);

Q_SIGNALS:
    void sessionAuthenticated(const  QJSValue &reply) const;
    void sessionAuthenticationError(const  QJSValue &reply) const;
    void sessionTerminated() const;
    void finished(const QJSValue &reply);
    void error(const QJSValue &reply);

private:
    Q_DECLARE_PRIVATE(EnginioQmlClient)
};

QT_END_NAMESPACE

#endif // ENGINIOQMLCLIENT_H

