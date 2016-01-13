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

#include <Enginio/enginioidentity.h>
#include <Enginio/enginiooauth2authentication.h>
#include <Enginio/private/enginioclient_p.h>
#include <Enginio/enginioreply.h>

#include <QtCore/qjsonobject.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qstring.h>
#include <QtNetwork/qnetworkreply.h>

QT_BEGIN_NAMESPACE

/*!
  \class EnginioIdentity
  \since 5.3
  \inmodule enginio-qt
  \ingroup enginio-client
  \brief Represents a user that is authenticated with the backend
  This class is an abstract base class for the different authentication methods and is never used directly.
*/

/*!
  \fn EnginioIdentity::dataChanged()
  \internal
*/

/*!
  \fn EnginioIdentity::aboutToDestroy()
  \internal
*/

class EnginioIdentityPrivate : public QObjectPrivate {};

/*!
    Constructs a new EnginioIdentity with \a dd pointer \a parent as QObject parent.
    \internal
*/
EnginioIdentity::EnginioIdentity(EnginioIdentityPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{}

/*!
    \fn EnginioIdentity::prepareSessionToken(EnginioClientConnectionPrivate *enginio);
    \brief EnginioIdentity::prepareSessionToken is called by enginio client to initialize authentication
    \param enginio client pointer
    \internal
*/
/*!
    \fn EnginioIdentity::removeSessionToken(EnginioClientConnectionPrivate *enginio);
    \brief This method is called by enginio client to finalize de-authentication
    \param enginio client pointer
    \internal
*/

class EnginioUserPassAuthenticationPrivate;
struct DisconnectConnection
{
    EnginioUserPassAuthenticationPrivate *auth;

    DisconnectConnection(EnginioUserPassAuthenticationPrivate *authentication)
        : auth(authentication)
    {}

    inline void operator ()() const;
};

class EnginioUserPassAuthenticationPrivate : public EnginioIdentityPrivate
{
    template<typename T>
    class SessionSetterFunctor
    {
        EnginioClientConnectionPrivate *_enginio;
        QNetworkReply *_reply;
        EnginioUserPassAuthenticationPrivate *_auth;
    public:
        SessionSetterFunctor(EnginioClientConnectionPrivate *enginio, QNetworkReply *reply, EnginioUserPassAuthenticationPrivate *auth)
            : _enginio(enginio)
            , _reply(reply)
            , _auth(auth)
        {}
        void operator ()()
        {
            EnginioReplyState *ereply = _enginio->createReply(_reply);
            if (_reply->error() != QNetworkReply::NoError) {
                emit _enginio->emitSessionAuthenticationError(ereply);
            } else {
                _auth->thisAs<T>()->proccessToken(_enginio, ereply);
                _enginio->emitSessionAuthenticated(ereply);
            }
        }
    };

    QPointer<QNetworkReply> _reply;
    QMetaObject::Connection _replyFinished;
    QMetaObject::Connection _enginioDestroyed;

public:
    QString _user;
    QString _pass;

    ~EnginioUserPassAuthenticationPrivate();

    template<class Derived>
    Derived *thisAs() { return static_cast<Derived*>(this); }

    void cleanupConnections()
    {
        if (_reply) {
            QObject::disconnect(_replyFinished);
            QObject::disconnect(_enginioDestroyed);
            QObject::connect(_reply.data(), &QNetworkReply::finished, _reply.data(), &QNetworkReply::deleteLater);
            _reply = 0;
        }
    }

    template<typename Derived>
    void prepareSessionToken(EnginioClientConnectionPrivate *enginio)
    {
        cleanupConnections();

        _reply = thisAs<Derived>()->makeRequest(enginio);
        enginio->setAuthenticationState(Enginio::Authenticating);
        _replyFinished = QObject::connect(_reply.data(), &QNetworkReply::finished, SessionSetterFunctor<Derived>(enginio, _reply.data(), this));
        _enginioDestroyed = QObject::connect(enginio->q_ptr, &EnginioClient::destroyed, DisconnectConnection(this));
    }

    template<typename Derived>
    void removeSessionToken(EnginioClientConnectionPrivate *enginio)
    {
        cleanupConnections();
        thisAs<Derived>()->cleanupClient(enginio);
        _reply = 0;
        enginio->emitSessionTerminated();
    }
};

class EnginioOAuth2AuthenticationPrivate: public EnginioUserPassAuthenticationPrivate
{
public:
    QNetworkReply *makeRequest(EnginioClientConnectionPrivate *enginio)
    {
        QByteArray data;
        {
            QUrlQuery urlQuery;
            urlQuery.addQueryItem(EnginioString::grant_type, EnginioString::password);
            urlQuery.addQueryItem(EnginioString::username, _user);
            urlQuery.addQueryItem(EnginioString::password, _pass);
            data = urlQuery.query().toUtf8();
        }

        QUrl url(enginio->_serviceUrl);
        url.setPath(EnginioString::v1_auth_oauth2_token);

        QNetworkRequest request(enginio->prepareRequest(url));
        request.setHeader(QNetworkRequest::ContentTypeHeader, EnginioString::Application_x_www_form_urlencoded);
        request.setRawHeader(EnginioString::Accept, EnginioString::Application_json);

        return enginio->networkManager()->post(request, data);
    }

    void proccessToken(EnginioClientConnectionPrivate *enginio, EnginioReplyState *ereply)
    {
        QByteArray header;
        header = EnginioString::Bearer_ + ereply->data()[EnginioString::access_token].toString().toUtf8();
        enginio->_request.setRawHeader(EnginioString::Authorization, header);
    }

    void cleanupClient(EnginioClientConnectionPrivate *enginio)
    {
        enginio->_request.setRawHeader(EnginioString::Authorization, QByteArray());
    }
};

/*!
  \class EnginioOAuth2Authentication
  \since 5.3
  \inmodule enginio-qt
  \ingroup enginio-client
  \brief Represents a user that is authenticated directly by the backend using OAuth2 standard.

  This class can authenticate a user by verifying the user's login and password.
  The user has to exist in the backend already.

  To authenticate an instance of EnginioClient called \a client such code may be used:
  \code
    EnginioOAuth2Authentication *identity = new EnginioOAuth2Authentication(client);
    identity->setUser(_user);
    identity->setPassword(_password);

    client->setIdentity(identity);
  \endcode

  Setting the identity will trigger an asynchronous request, resulting in EnginioClient::authenticationState()
  changing.

  \sa EnginioClientConnection::authenticationState() EnginioClientConnection::identity() EnginioClient::sessionAuthenticated()
  \sa EnginioClient::sessionAuthenticationError() EnginioClient::sessionTerminated()
*/

/*!
  Constructs a EnginioPasswordOAuth2 instance with \a parent as QObject parent.
*/
EnginioOAuth2Authentication::EnginioOAuth2Authentication(QObject *parent)
    : EnginioIdentity(*new EnginioOAuth2AuthenticationPrivate(), parent)
{
    connect(this, &EnginioOAuth2Authentication::userChanged, this, &EnginioIdentity::dataChanged);
    connect(this, &EnginioOAuth2Authentication::passwordChanged, this, &EnginioIdentity::dataChanged);
}

/*!
  Destructs this EnginioPasswordOAuth2 instance.
*/
EnginioOAuth2Authentication::~EnginioOAuth2Authentication()
{
    emit aboutToDestroy();
}

/*!
  \property EnginioOAuth2Authentication::user
  This property contains the user name used for authentication.
*/

QString EnginioOAuth2Authentication::user() const
{
    Q_D(const EnginioOAuth2Authentication);
    return d->_user;
}

void EnginioOAuth2Authentication::setUser(const QString &user)
{
    Q_D(EnginioOAuth2Authentication);
    if (d->_user == user)
        return;
    d->_user = user;
    emit userChanged(user);
}

/*!
  \property EnginioOAuth2Authentication::password
  This property contains the password used for authentication.
*/

QString EnginioOAuth2Authentication::password() const
{
    Q_D(const EnginioOAuth2Authentication);
    return d->_pass;
}

void EnginioOAuth2Authentication::setPassword(const QString &password)
{
    Q_D(EnginioOAuth2Authentication);
    if (d->_pass == password)
        return;
    d->_pass = password;
    emit passwordChanged(password);
}

/*!
  \internal
*/
void EnginioOAuth2Authentication::prepareSessionToken(EnginioClientConnectionPrivate *enginio)
{
    Q_ASSERT(enginio);
    Q_ASSERT(enginio->identity());
    Q_D(EnginioOAuth2Authentication);
    d->prepareSessionToken<EnginioOAuth2AuthenticationPrivate>(enginio);
}

/*!
  \internal
*/
void EnginioOAuth2Authentication::removeSessionToken(EnginioClientConnectionPrivate *enginio)
{
    Q_ASSERT(enginio);
    Q_ASSERT(enginio->identity());
    Q_D(EnginioOAuth2Authentication);
    d->removeSessionToken<EnginioOAuth2AuthenticationPrivate>(enginio);
}

void DisconnectConnection::operator ()() const
{
    auth->cleanupConnections();
}

EnginioUserPassAuthenticationPrivate::~EnginioUserPassAuthenticationPrivate()
{
    cleanupConnections();
}

QT_END_NAMESPACE
