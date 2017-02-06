/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Network Auth module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QT_NO_HTTP

#include <qoauth2authorizationcodeflow.h>
#include <private/qoauth2authorizationcodeflow_p.h>

#include <qmap.h>
#include <qurl.h>
#include <qvariant.h>
#include <qurlquery.h>
#include <qjsonobject.h>
#include <qjsondocument.h>
#include <qauthenticator.h>
#include <qoauthhttpserverreplyhandler.h>

#include <functional>

QT_BEGIN_NAMESPACE

QOAuth2AuthorizationCodeFlowPrivate::QOAuth2AuthorizationCodeFlowPrivate(
        const QUrl &authorizationUrl, const QUrl &accessTokenUrl, const QString &clientIdentifier,
        QNetworkAccessManager *manager) :
    QAbstractOAuth2Private(qMakePair(clientIdentifier, QString()), authorizationUrl, manager),
    accessTokenUrl(accessTokenUrl)
{}

QOAuth2AuthorizationCodeFlowPrivate::QOAuth2AuthorizationCodeFlowPrivate(
        QNetworkAccessManager *manager) : QAbstractOAuth2Private(manager)
{}

void QOAuth2AuthorizationCodeFlowPrivate::_q_handleCallback(const QVariantMap &data)
{
    Q_Q(QOAuth2AuthorizationCodeFlow);
    using Key = QAbstractOAuth2Private::OAuth2KeyString;

    if (status != QAbstractOAuth::Status::NotAuthenticated) {
        qWarning("QOAuth2AuthorizationCodeFlow: Unexpected call");
        return;
    }

    Q_ASSERT(!state.isEmpty());

    const QString error = data.value(Key::error).toString();
    const QString code = data.value(Key::code).toString();
    const QString receivedState = data.value(Key::state).toString();
    if (error.size()) {
        const QString uri = data.value(Key::errorUri).toString();
        const QString description = data.value(Key::errorDescription).toString();
        qWarning("QOAuth2AuthorizationCodeFlow: AuthenticationError: %s(%s): %s",
                 qPrintable(error), qPrintable(uri), qPrintable(description));
        Q_EMIT q->error(error, description, uri);
        return;
    }
    if (code.isEmpty()) {
        qWarning("QOAuth2AuthorizationCodeFlow: AuthenticationError: Code not received");
        return;
    }
    if (receivedState.isEmpty()) {
        qWarning("QOAuth2AuthorizationCodeFlow: State not received");
        return;
    }
    if (state != receivedState) {
        qWarning("QOAuth2AuthorizationCodeFlow: State mismatch");
        return;
    }

    setStatus(QAbstractOAuth::Status::TemporaryCredentialsReceived);

    QVariantMap copy(data);
    copy.remove(Key::code);
    extraTokens = copy;
    q->requestAccessToken(code);
}

void QOAuth2AuthorizationCodeFlowPrivate::_q_accessTokenRequestFinished(const QVariantMap &values)
{
    Q_Q(QOAuth2AuthorizationCodeFlow);
    using Key = QAbstractOAuth2Private::OAuth2KeyString;

    if (values.contains(Key::error)) {
        const QString error = values.value(Key::error).toString();
        qWarning("QOAuth2AuthorizationCodeFlow Error: %s", qPrintable(error));
        return;
    }

    bool ok;
    const QString accessToken = values.value(Key::accessToken).toString();
    tokenType = values.value(Key::tokenType).toString();
    int expiresIn = values.value(Key::expiresIn).toInt(&ok);
    if (!ok)
        expiresIn = -1;
    refreshToken = values.value(Key::refreshToken).toString();
    scope = values.value(Key::scope).toString();
    if (accessToken.isEmpty()) {
        qWarning("QOAuth2AuthorizationCodeFlow: Access token not received");
        return;
    }
    q->setToken(accessToken);

    const QDateTime currentDateTime = QDateTime::currentDateTime();
    if (expiresIn > 0 && currentDateTime.secsTo(expiresAt) != expiresIn) {
        expiresAt = currentDateTime.addSecs(expiresIn);
        Q_EMIT q->expirationAtChanged(expiresAt);
    }

    setStatus(QAbstractOAuth::Status::Granted);
}

void QOAuth2AuthorizationCodeFlowPrivate::_q_authenticate(QNetworkReply *reply,
                                                          QAuthenticator *authenticator)
{
    if (reply == currentReply){
        const auto url = reply->url();
        if (url == accessTokenUrl) {
            authenticator->setUser(clientCredentials.first);
            authenticator->setPassword(QString());
        }
    }
}

QOAuth2AuthorizationCodeFlow::QOAuth2AuthorizationCodeFlow(QObject *parent) :
    QAbstractOAuth2(*new QOAuth2AuthorizationCodeFlowPrivate, parent)
{}

QOAuth2AuthorizationCodeFlow::QOAuth2AuthorizationCodeFlow(QNetworkAccessManager *manager,
                                                           QObject *parent) :
    QAbstractOAuth2(*new QOAuth2AuthorizationCodeFlowPrivate(manager), parent)
{}

QOAuth2AuthorizationCodeFlow::QOAuth2AuthorizationCodeFlow(const QString &clientIdentifier,
                                                           QNetworkAccessManager *manager,
                                                           QObject *parent) :
    QAbstractOAuth2(*new QOAuth2AuthorizationCodeFlowPrivate(QUrl(), QUrl(), clientIdentifier,
                                                             manager),
                    parent)
{}

QOAuth2AuthorizationCodeFlow::QOAuth2AuthorizationCodeFlow(const QUrl &authenticateUrl,
                                                           const QUrl &accessTokenUrl,
                                                           QNetworkAccessManager *manager,
                                                           QObject *parent) :
    QAbstractOAuth2(*new QOAuth2AuthorizationCodeFlowPrivate(authenticateUrl, accessTokenUrl,
                                                             QString(), manager),
                    parent)
{}

QOAuth2AuthorizationCodeFlow::QOAuth2AuthorizationCodeFlow(const QString &clientIdentifier,
                                                           const QUrl &authenticateUrl,
                                                           const QUrl &accessTokenUrl,
                                                           QNetworkAccessManager *manager,
                                                           QObject *parent) :
    QAbstractOAuth2(*new QOAuth2AuthorizationCodeFlowPrivate(authenticateUrl, accessTokenUrl,
                                                             clientIdentifier, manager),
                    parent)
{}

QOAuth2AuthorizationCodeFlow::~QOAuth2AuthorizationCodeFlow()
{}

QString QOAuth2AuthorizationCodeFlow::responseType() const
{
    return QStringLiteral("code");
}

QUrl QOAuth2AuthorizationCodeFlow::accessTokenUrl() const
{
    Q_D(const QOAuth2AuthorizationCodeFlow);
    return d->accessTokenUrl;
}

void QOAuth2AuthorizationCodeFlow::setAccessTokenUrl(const QUrl &accessTokenUrl)
{
    Q_D(QOAuth2AuthorizationCodeFlow);
    if (d->accessTokenUrl != accessTokenUrl) {
        d->accessTokenUrl = accessTokenUrl;
        Q_EMIT accessTokenUrlChanged(accessTokenUrl);
    }
}

void QOAuth2AuthorizationCodeFlow::grant()
{
    Q_D(QOAuth2AuthorizationCodeFlow);
    if (d->authorizationUrl.isEmpty()) {
        qWarning("QOAuth2AuthorizationCodeFlow::grant: No authenticate Url set");
        return;
    }
    if (d->accessTokenUrl.isEmpty()) {
        qWarning("QOAuth2AuthorizationCodeFlow::grant: No request access token Url set");
        return;
    }

    resourceOwnerAuthorization(d->authorizationUrl);
}

void QOAuth2AuthorizationCodeFlow::refreshAccessToken()
{
    Q_D(QOAuth2AuthorizationCodeFlow);

    if (d->refreshToken.isEmpty()) {
        qWarning("QOAuth2AuthorizationCodeFlow::refreshAccessToken: Cannot refresh access token. "
                 "Empty refresh token");
        return;
    }
    if (d->status == Status::RefreshingToken) {
        qWarning("QOAuth2AuthorizationCodeFlow::refreshAccessToken: Cannot refresh access token. "
                 "Refresh Access Token in progress");
        return;
    }

    using Key = QAbstractOAuth2Private::OAuth2KeyString;

    QVariantMap parameters;
    QNetworkRequest request(d->accessTokenUrl);
    QUrlQuery query;
    parameters.insert(Key::grantType, QStringLiteral("refresh_token"));
    parameters.insert(Key::refreshToken, d->refreshToken);
    parameters.insert(Key::redirectUri, QUrl::toPercentEncoding(callback()));
    query = QAbstractOAuthPrivate::createQuery(parameters);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

    const QString data = query.toString(QUrl::FullyEncoded);
    d->currentReply = d->networkAccessManager()->post(request, data.toUtf8());
    d->status = Status::RefreshingToken;

    connect(d->currentReply.data(), &QNetworkReply::finished,
            std::bind(&QAbstractOAuthReplyHandler::networkReplyFinished, replyHandler(),
                      d->currentReply.data()));
    connect(d->currentReply.data(), &QNetworkReply::finished, d->currentReply.data(),
            &QNetworkReply::deleteLater);
    QObjectPrivate::connect(d->networkAccessManager(),
                            &QNetworkAccessManager::authenticationRequired,
                            d, &QOAuth2AuthorizationCodeFlowPrivate::_q_authenticate,
                            Qt::UniqueConnection);
}

QUrl QOAuth2AuthorizationCodeFlow::buildAuthenticateUrl(const QVariantMap &parameters)
{
    Q_D(QOAuth2AuthorizationCodeFlow);
    using Key = QAbstractOAuth2Private::OAuth2KeyString;

    if (d->state.isEmpty())
        setState(QAbstractOAuth2Private::generateRandomState());
    Q_ASSERT(!d->state.isEmpty());
    const QString state = d->state;

    QVariantMap p(parameters);
    QUrl url(d->authorizationUrl);
    p.insert(Key::responseType, responseType());
    p.insert(Key::clientIdentifier, d->clientCredentials.first);
    p.insert(Key::redirectUri, callback());
    p.insert(Key::scope, d->scope);
    p.insert(Key::state, state);
    if (d->modifyParametersFunction)
        d->modifyParametersFunction(Stage::RequestingAuthorization, &p);
    url.setQuery(d->createQuery(p));
    connect(d->replyHandler.data(), &QAbstractOAuthReplyHandler::callbackReceived, this,
            &QOAuth2AuthorizationCodeFlow::authorizationCallbackReceived, Qt::UniqueConnection);
    setStatus(QAbstractOAuth::Status::NotAuthenticated);
    qDebug("QOAuth2AuthorizationCodeFlow::buildAuthenticateUrl: %s", qPrintable(url.toString()));
    return url;
}

void QOAuth2AuthorizationCodeFlow::requestAccessToken(const QString &code)
{
    Q_D(QOAuth2AuthorizationCodeFlow);
    using Key = QAbstractOAuth2Private::OAuth2KeyString;

    QVariantMap parameters;
    QNetworkRequest request(d->accessTokenUrl);
    QUrlQuery query;
    parameters.insert(Key::grantType, QStringLiteral("authorization_code"));
    parameters.insert(Key::code, QUrl::toPercentEncoding(code));
    parameters.insert(Key::redirectUri, QUrl::toPercentEncoding(callback()));
    parameters.insert(Key::clientIdentifier, QUrl::toPercentEncoding(d->clientCredentials.first));
    if (!d->clientCredentials.second.isEmpty())
        parameters.insert(Key::clientSharedSecret, d->clientCredentials.second);
    if (d->modifyParametersFunction)
        d->modifyParametersFunction(Stage::RequestingAccessToken, &parameters);
    query = QAbstractOAuthPrivate::createQuery(parameters);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

    const QString data = query.toString(QUrl::FullyEncoded);
    d->currentReply = d->networkAccessManager()->post(request, data.toUtf8());
    QObject::connect(d->currentReply.data(), &QNetworkReply::finished,
                     std::bind(&QAbstractOAuthReplyHandler::networkReplyFinished, replyHandler(),
                               d->currentReply.data()));
    QObjectPrivate::connect(d->replyHandler.data(), &QAbstractOAuthReplyHandler::tokensReceived, d,
                            &QOAuth2AuthorizationCodeFlowPrivate::_q_accessTokenRequestFinished,
                            Qt::UniqueConnection);
    QObjectPrivate::connect(d->networkAccessManager(),
                            &QNetworkAccessManager::authenticationRequired,
                            d, &QOAuth2AuthorizationCodeFlowPrivate::_q_authenticate,
                            Qt::UniqueConnection);
}

void QOAuth2AuthorizationCodeFlow::resourceOwnerAuthorization(const QUrl &url,
                                                              const QVariantMap &parameters)
{
    Q_D(QOAuth2AuthorizationCodeFlow);
    if (Q_UNLIKELY(url != d->authorizationUrl)) {
        qWarning("Invalid URL: %s", qPrintable(url.toString()));
        return;
    }
    const QUrl u = buildAuthenticateUrl(parameters);
    QObjectPrivate::connect(this, &QOAuth2AuthorizationCodeFlow::authorizationCallbackReceived, d,
                            &QOAuth2AuthorizationCodeFlowPrivate::_q_handleCallback,
                            Qt::UniqueConnection);
    Q_EMIT authorizeWithBrowser(u);
}

QT_END_NAMESPACE

#endif // QT_NO_HTTP
