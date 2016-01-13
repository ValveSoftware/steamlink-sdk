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

#include "enginioplugin_p.h"
#include "enginioqmlclient_p.h"
#include "enginioqmlmodel_p.h"
#include "enginioqmlreply_p.h"
#include <Enginio/enginiomodel.h>
#include <Enginio/enginioreply.h>
#include <Enginio/enginioidentity.h>
#include <Enginio/enginiooauth2authentication.h>
#include <Enginio/private/enginioclient_p.h>

#include <qqml.h>
#include <QtQml/qqmlnetworkaccessmanagerfactory.h>
#include <QtQml/qqmlengine.h>

QT_BEGIN_NAMESPACE

/*!
 \qmlmodule Enginio 1.0
 \title Enginio QML Plugin

 The Enginio QML plugin provides access to the Enginio service through a set of
 QML types.
 */

class EnginioNetworkAccessManagerFactory: public QQmlNetworkAccessManagerFactory
{
public:
    virtual QNetworkAccessManager *create(QObject *parent) Q_DECL_OVERRIDE
    {
        // Make sure we use the parent to remove the reference but not actually destroy
        // the QNAM.
        QNetworkAccessManagerHolder *holder = new QNetworkAccessManagerHolder(parent);
        holder->_guard = EnginioClientConnectionPrivate::prepareNetworkManagerInThread();
        return holder->_guard.data();
    }
};

void EnginioPlugin::initializeEngine(QQmlEngine *engine, const char *uri)
{
    Q_UNUSED(uri);

    if (!engine->networkAccessManagerFactory()) {
        static EnginioNetworkAccessManagerFactory factory;
        engine->setNetworkAccessManagerFactory(&factory);
    } else {
        qWarning() << "Enginio client failed to install QQmlNetworkAccessManagerFactory"
                      "on QML engine because a different factory is already attached, it"
                      " is recommended to use QNetworkAccessManager delivered by Enginio";
    }
}

/*!
  \qmltype EnginioOAuth2Authentication
  \since 5.3
  \instantiates EnginioOAuth2Authentication
  \inqmlmodule Enginio
  \ingroup engino-qml
  \target EnginioOAuth2AuthenticationQml
  \brief Represents a user that is authenticated directly by the backend using OAuth2 standard.

  This component can authenticate a user by verifying the user's login and password.
  The user has to exist in the backend already.

  To authenticate an instance of EnginioClient called \a client such code may be used:
  \code
    EnginioClient {
        ...
        identity: oauth2
    }
    EnginioOAuth2Authentication {
        id: oauth2
        user: "userName"
        password: "userPassword"
    }
  \endcode

  Setting the identity on the EnginioClient will trigger an asynchronous request, resulting in
  EnginioClient::authenticationState changing.

  \sa EnginioClient::authenticationState EnginioClient::identity EnginioClient::sessionAuthenticated
  \sa EnginioClient::sessionAuthenticationError() EnginioClient::sessionTerminated()
*/

/*!
  \qmlproperty EnginioOAuth2Authentication::user
  This property contains the user name used for authentication.
*/

/*!
  \qmlproperty EnginioOAuth2Authentication::password
  This property contains the password used for authentication.
*/


void EnginioPlugin::registerTypes(const char *uri)
{
    // @uri Enginio
    qmlRegisterUncreatableType<Enginio>(uri, 1, 0, "Enginio", "Enginio is an enum container and can not be constructed");
    qmlRegisterUncreatableType<EnginioClientConnection>(uri, 1, 0, "EnginioClientConnection", "EnginioClientConnection should not be instantiated in QML directly.");
    qmlRegisterType<EnginioQmlClient>(uri, 1, 0, "EnginioClient");
    qmlRegisterUncreatableType<EnginioBaseModel>(uri, 1, 0, "EnginioBaseModel", "EnginioBaseModel should not be instantiated in QML directly.");
    qmlRegisterType<EnginioQmlModel>(uri, 1, 0, "EnginioModel");
    qmlRegisterUncreatableType<EnginioReplyState>(uri, 1, 0, "EnginioReplyState", "EnginioReplyState cannot be instantiated.");
    qmlRegisterUncreatableType<EnginioQmlReply>(uri, 1, 0, "EnginioReply", "EnginioReply cannot be instantiated.");
    qmlRegisterUncreatableType<EnginioIdentity>(uri, 1, 0, "EnginioIdentity", "EnginioIdentity can not be instantiated directly");
    qmlRegisterType<EnginioOAuth2Authentication>(uri, 1, 0, "EnginioOAuth2Authentication");
    qmlRegisterUncreatableType<QNetworkReply>(uri, 1, 0, "QNetworkReply", "QNetworkReply is abstract and it can not be constructed");
}

QT_END_NAMESPACE
