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

#include "qoauth1.h"
#include "qoauth1_p.h"
#include "qoauth1signature.h"
#include "qoauthoobreplyhandler.h"
#include "qoauthhttpserverreplyhandler.h"

#include <QtCore/qmap.h>
#include <QtCore/qlist.h>
#include <QtCore/qvariant.h>
#include <QtCore/qurlquery.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qmessageauthenticationcode.h>

#include <QtNetwork/qnetworkreply.h>
#include <QtNetwork/qnetworkrequest.h>
#include <QtNetwork/qnetworkaccessmanager.h>

QT_BEGIN_NAMESPACE

using Key = QOAuth1Private::OAuth1KeyString;
const QString Key::oauthCallback =           QStringLiteral("oauth_callback");
const QString Key::oauthCallbackConfirmed =  QStringLiteral("oauth_callback_confirmed");
const QString Key::oauthConsumerKey =        QStringLiteral("oauth_consumer_key");
const QString Key::oauthNonce =              QStringLiteral("oauth_nonce");
const QString Key::oauthSignature =          QStringLiteral("oauth_signature");
const QString Key::oauthSignatureMethod =    QStringLiteral("oauth_signature_method");
const QString Key::oauthTimestamp =          QStringLiteral("oauth_timestamp");
const QString Key::oauthToken =              QStringLiteral("oauth_token");
const QString Key::oauthTokenSecret =        QStringLiteral("oauth_token_secret");
const QString Key::oauthVerifier =           QStringLiteral("oauth_verifier");
const QString Key::oauthVersion =            QStringLiteral("oauth_version");

QOAuth1Private::QOAuth1Private(const QPair<QString, QString> &clientCredentials,
                               QNetworkAccessManager *networkAccessManager) :
    QAbstractOAuthPrivate(networkAccessManager),
    clientCredentials(clientCredentials)
{
    qRegisterMetaType<QNetworkReply::NetworkError>("QNetworkReply::NetworkError");
}

void QOAuth1Private::appendCommonHeaders(QVariantMap *headers)
{
    const auto currentDateTime = QDateTime::currentDateTimeUtc();

    headers->insert(Key::oauthNonce, QOAuth1::nonce());
    headers->insert(Key::oauthConsumerKey, clientCredentials.first);
    headers->insert(Key::oauthTimestamp, QString::number(currentDateTime.toTime_t()));
    headers->insert(Key::oauthVersion, oauthVersion);
    headers->insert(Key::oauthSignatureMethod, signatureMethodString().toUtf8());
}

void QOAuth1Private::appendSignature(QAbstractOAuth::Stage stage,
                                     QVariantMap *headers,
                                     const QUrl &url,
                                     QNetworkAccessManager::Operation operation,
                                     const QVariantMap parameters)
{
    QByteArray signature;
    {
        QVariantMap allParameters = QVariantMap(*headers).unite(parameters);
        if (modifyParametersFunction)
            modifyParametersFunction(stage, &allParameters);
        signature = generateSignature(allParameters, url, operation);
    }
    headers->insert(Key::oauthSignature, signature);
}

QNetworkReply *QOAuth1Private::requestToken(QNetworkAccessManager::Operation operation,
                                            const QUrl &url,
                                            const QPair<QString, QString> &token,
                                            const QVariantMap &parameters)
{
    Q_Q(QOAuth1);
    if (Q_UNLIKELY(!networkAccessManager())) {
        qWarning("QOAuth1Private::requestToken: QNetworkAccessManager not available");
        return nullptr;
    }
    if (Q_UNLIKELY(url.isEmpty())) {
        qWarning("QOAuth1Private::requestToken: Request Url not set");
        return nullptr;
    }
    if (Q_UNLIKELY(operation != QNetworkAccessManager::GetOperation &&
                   operation != QNetworkAccessManager::PostOperation)) {
        qWarning("QOAuth1Private::requestToken: Operation not supported");
        return nullptr;
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    QAbstractOAuth::Stage stage = QAbstractOAuth::Stage::RequestingTemporaryCredentials;
    QVariantMap headers;
    appendCommonHeaders(&headers);
    headers.insert(Key::oauthCallback, q->callback());
    if (!token.first.isEmpty()) {
        headers.insert(Key::oauthToken, token.first);
        stage = QAbstractOAuth::Stage::RequestingAccessToken;
    }
    appendSignature(stage, &headers, url, operation, parameters);

    request.setRawHeader("Authorization", q->generateAuthorizationHeader(headers));

    QNetworkReply *reply = nullptr;
    if (operation == QNetworkAccessManager::GetOperation) {
        if (parameters.size() > 0) {
            QUrl url = request.url();
            url.setQuery(QOAuth1Private::createQuery(parameters));
            request.setUrl(url);
        }
        reply = networkAccessManager()->get(request);
    }
    else if (operation == QNetworkAccessManager::PostOperation) {
        QUrlQuery query = QOAuth1Private::createQuery(parameters);
        const QByteArray data = query.toString(QUrl::FullyEncoded).toUtf8();
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QStringLiteral("application/x-www-form-urlencoded"));
        reply = networkAccessManager()->post(request, data);
    }

    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
            this, &QOAuth1Private::_q_onTokenRequestError);

    QAbstractOAuthReplyHandler *handler = replyHandler ? replyHandler.data()
                                                       : defaultReplyHandler.data();
    QObject::connect(reply, &QNetworkReply::finished,
                     std::bind(&QAbstractOAuthReplyHandler::networkReplyFinished, handler, reply));
    connect(handler, &QAbstractOAuthReplyHandler::tokensReceived, this,
            &QOAuth1Private::_q_tokensReceived);

    return reply;
}

QString QOAuth1Private::signatureMethodString() const
{
    switch (signatureMethod) { // No default: intended
    case QOAuth1::SignatureMethod::PlainText:
        return QStringLiteral("PLAINTEXT");
    case QOAuth1::SignatureMethod::Hmac_Sha1:
        return QStringLiteral("HMAC-SHA1");
    case QOAuth1::SignatureMethod::Rsa_Sha1:
        qFatal("RSA-SHA1 signature method not supported");
        return QStringLiteral("RSA-SHA1");
    }
    qFatal("Invalid signature method");
    return QString();
}

QByteArray QOAuth1Private::generateSignature(const QVariantMap &parameters,
                                             const QUrl &url,
                                             QNetworkAccessManager::Operation operation) const
{
    const QOAuth1Signature signature(url,
                                     clientCredentials.second,
                                     tokenCredentials.second,
                                     static_cast<QOAuth1Signature::HttpRequestMethod>(operation),
                                     parameters);

    switch (signatureMethod) {
    case QOAuth1::SignatureMethod::Hmac_Sha1:
        return signature.hmacSha1().toBase64();
    case QOAuth1::SignatureMethod::PlainText:
        return signature.plainText(clientCredentials.first);
    default:
        qFatal("QOAuth1Private::generateSignature: Signature method not supported");
        return QByteArray();
    }
}

void QOAuth1Private::_q_onTokenRequestError(QNetworkReply::NetworkError error)
{
    Q_Q(QOAuth1);
    Q_UNUSED(error);
    Q_EMIT q->requestFailed(QAbstractOAuth::Error::NetworkError);
}

void QOAuth1Private::_q_tokensReceived(const QVariantMap &tokens)
{
    Q_Q(QOAuth1);

    QPair<QString, QString> credential(tokens.value(Key::oauthToken).toString(),
                                       tokens.value(Key::oauthTokenSecret).toString());
    switch (status) {
    case QAbstractOAuth::Status::NotAuthenticated:
        if (tokens.value(Key::oauthCallbackConfirmed, true).toBool()) {
            q->setTokenCredentials(credential);
            setStatus(QAbstractOAuth::Status::TemporaryCredentialsReceived);
        } else {
            Q_EMIT q->requestFailed(QAbstractOAuth::Error::OAuthCallbackNotVerified);
        }
        break;
    case QAbstractOAuth::Status::TemporaryCredentialsReceived:
        q->setTokenCredentials(credential);
        setStatus(QAbstractOAuth::Status::Granted);
        break;
    case QAbstractOAuth::Status::Granted:
    case QAbstractOAuth::Status::RefreshingToken:
        break;
    }
}

QOAuth1::QOAuth1(QObject *parent) :
    QOAuth1(nullptr,
            parent)
{}

QOAuth1::QOAuth1(QNetworkAccessManager *manager, QObject *parent) :
    QOAuth1(QString(),
            QString(),
            manager,
            parent)
{}

QOAuth1::QOAuth1(const QString &clientIdentifier,
                 const QString &clientSharedSecret,
                 QNetworkAccessManager *manager,
                 QObject *parent)
    : QAbstractOAuth(*new QOAuth1Private(qMakePair(clientIdentifier, clientSharedSecret),
                                         manager),
                     parent)
{}

QOAuth1::~QOAuth1()
{}

QString QOAuth1::clientIdentifier() const
{
    Q_D(const QOAuth1);
    return d->clientCredentials.first;
}

void QOAuth1::setClientIdentifier(const QString &clientIdentifier)
{
    Q_D(QOAuth1);
    if (d->clientCredentials.first != clientIdentifier) {
        d->clientCredentials.first = clientIdentifier;
        Q_EMIT clientIdentifierChanged(clientIdentifier);
    }
}

QString QOAuth1::clientSharedSecret() const
{
    Q_D(const QOAuth1);
    return d->clientCredentials.second;
}

void QOAuth1::setClientSharedSecret(const QString &clientSharedSecret)
{
    Q_D(QOAuth1);
    if (d->clientCredentials.second != clientSharedSecret) {
        d->clientCredentials.second = clientSharedSecret;
        Q_EMIT clientSharedSecretChanged(clientSharedSecret);
    }
}

QPair<QString, QString> QOAuth1::clientCredentials() const
{
    Q_D(const QOAuth1);
    return d->clientCredentials;
}

void QOAuth1::setClientCredentials(const QPair<QString, QString> &clientCredentials)
{
    setClientCredentials(clientCredentials.first, clientCredentials.second);
}

void QOAuth1::setClientCredentials(const QString &clientIdentifier,
                                   const QString &clientSharedSecret)
{
    setClientIdentifier(clientIdentifier);
    setClientSharedSecret(clientSharedSecret);
}

QString QOAuth1::token() const
{
    Q_D(const QOAuth1);
    return d->tokenCredentials.first;
}

void QOAuth1::setToken(const QString &token)
{
    Q_D(QOAuth1);
    if (d->tokenCredentials.first != token) {
        d->tokenCredentials.first = token;
        Q_EMIT tokenChanged(token);
    }
}

QString QOAuth1::tokenSecret() const
{
    Q_D(const QOAuth1);
    return d->tokenCredentials.second;
}

void QOAuth1::setTokenSecret(const QString &tokenSecret)
{
    Q_D(QOAuth1);
    if (d->tokenCredentials.second != tokenSecret) {
        d->tokenCredentials.second = tokenSecret;
        Q_EMIT tokenSecretChanged(tokenSecret);
    }
}

QPair<QString, QString> QOAuth1::tokenCredentials() const
{
    Q_D(const QOAuth1);
    return d->tokenCredentials;
}

void QOAuth1::setTokenCredentials(const QPair<QString, QString> &tokenCredentials)
{
    setTokenCredentials(tokenCredentials.first, tokenCredentials.second);
}

void QOAuth1::setTokenCredentials(const QString &token, const QString &tokenSecret)
{
    setToken(token);
    setTokenSecret(tokenSecret);
}

QUrl QOAuth1::temporaryCredentialsUrl() const
{
    Q_D(const QOAuth1);
    return d->temporaryCredentialsUrl;
}

void QOAuth1::setTemporaryCredentialsUrl(const QUrl &url)
{
    Q_D(QOAuth1);
    if (d->temporaryCredentialsUrl != url) {
        d->temporaryCredentialsUrl = url;
        Q_EMIT temporaryCredentialsUrlChanged(url);
    }
}

QUrl QOAuth1::tokenCredentialsUrl() const
{
    Q_D(const QOAuth1);
    return d->tokenCredentialsUrl;
}

void QOAuth1::setTokenCredentialsUrl(const QUrl &url)
{
    Q_D(QOAuth1);
    if (d->tokenCredentialsUrl != url) {
        d->tokenCredentialsUrl = url;
        Q_EMIT tokenCredentialsUrlChanged(url);
    }
}

QOAuth1::SignatureMethod QOAuth1::signatureMethod() const
{
    Q_D(const QOAuth1);
    return d->signatureMethod;
}

void QOAuth1::setSignatureMethod(QOAuth1::SignatureMethod value)
{
    Q_D(QOAuth1);
    if (d->signatureMethod != value) {
        d->signatureMethod = value;
        Q_EMIT signatureMethodChanged(value);
    }
}

QNetworkReply *QOAuth1::head(const QUrl &url, const QVariantMap &parameters)
{
    Q_D(QOAuth1);
    if (!d->networkAccessManager()) {
        qWarning("QOAuth1::head: QNetworkAccessManager not available");
        return nullptr;
    }
    QNetworkRequest request(url);
    setup(&request, parameters, QNetworkAccessManager::HeadOperation);
    return d->networkAccessManager()->head(request);
}

QNetworkReply *QOAuth1::get(const QUrl &url, const QVariantMap &parameters)
{
    Q_D(QOAuth1);
    if (!d->networkAccessManager()) {
        qWarning("QOAuth1::get: QNetworkAccessManager not available");
        return nullptr;
    }
    QNetworkRequest request(url);
    setup(&request, parameters, QNetworkAccessManager::GetOperation);
    QNetworkReply *reply = d->networkAccessManager()->get(request);
    connect(reply, &QNetworkReply::finished, std::bind(&QAbstractOAuth::finished, this, reply));
    return reply;
}

QNetworkReply *QOAuth1::post(const QUrl &url, const QVariantMap &parameters)
{
    Q_D(QOAuth1);
    if (!d->networkAccessManager()) {
        qWarning("QOAuth1::post: QNetworkAccessManager not available");
        return nullptr;
    }
    QNetworkRequest request(url);
    setup(&request, parameters, QNetworkAccessManager::PostOperation);

    QUrlQuery query;
    for (auto it = parameters.begin(), end = parameters.end(); it != end; ++it)
        query.addQueryItem(it.key(), it.value().toString());
    QString data = query.toString(QUrl::FullyEncoded);
    QNetworkReply *reply = d->networkAccessManager()->post(request, data.toUtf8());
    connect(reply, &QNetworkReply::finished, std::bind(&QAbstractOAuth::finished, this, reply));
    return reply;
}

QNetworkReply *QOAuth1::deleteResource(const QUrl &url, const QVariantMap &parameters)
{
    Q_D(QOAuth1);
    if (!d->networkAccessManager()) {
        qWarning("QOAuth1::deleteResource: QNetworkAccessManager not available");
        return nullptr;
    }
    QNetworkRequest request(url);
    setup(&request, parameters, QNetworkAccessManager::DeleteOperation);
    QNetworkReply *reply = d->networkAccessManager()->deleteResource(request);
    connect(reply, &QNetworkReply::finished, std::bind(&QAbstractOAuth::finished, this, reply));
    return reply;
}

QNetworkReply *QOAuth1::requestTemporaryCredentials(QNetworkAccessManager::Operation operation,
                                                    const QUrl &url,
                                                    const QVariantMap &parameters)
{
    // https://tools.ietf.org/html/rfc5849#section-2.1
    Q_D(QOAuth1);
    d->tokenCredentials = QPair<QString, QString>();
    return d->requestToken(operation, url, d->tokenCredentials, parameters);
}

QNetworkReply *QOAuth1::requestTokenCredentials(QNetworkAccessManager::Operation operation,
                                                const QUrl &url,
                                                const QPair<QString, QString> &temporaryToken,
                                                const QVariantMap &parameters)
{
    Q_D(QOAuth1);
    return d->requestToken(operation, url, temporaryToken, parameters);
}

void QOAuth1::setup(QNetworkRequest *request,
                    const QVariantMap &signingParameters,
                    QNetworkAccessManager::Operation operation)
{
    Q_D(const QOAuth1);

    QVariantMap oauthParams;
    QVariantMap otherParams = signingParameters;
    // Adding parameters located in the query
    {
        auto queryItems = QUrlQuery(request->url().query()).queryItems();
        for (auto it = queryItems.begin(), end = queryItems.end(); it != end; ++it)
            otherParams.insert(it->first, it->second);
    }

    const auto currentDateTime = QDateTime::currentDateTimeUtc();

    oauthParams.insert(Key::oauthConsumerKey, d->clientCredentials.first);
    oauthParams.insert(Key::oauthVersion, QStringLiteral("1.0"));
    oauthParams.insert(Key::oauthToken, d->tokenCredentials.first);
    oauthParams.insert(Key::oauthSignatureMethod, d->signatureMethodString());
    oauthParams.insert(Key::oauthNonce, QOAuth1::nonce());
    oauthParams.insert(Key::oauthTimestamp, QString::number(currentDateTime.toTime_t()));

    // Add signature parameter
    {
        const auto parameters = QVariantMap(oauthParams).unite(signingParameters);
        const auto signature = d->generateSignature(parameters, request->url(), operation);
        oauthParams.insert(Key::oauthSignature, signature);
    }

    if (operation == QNetworkAccessManager::GetOperation) {
        if (signingParameters.size()) {
            QUrl url = request->url();
            QUrlQuery query;
            for (auto it = signingParameters.begin(), end = signingParameters.end(); it != end;
                 ++it)
                query.addQueryItem(it.key(), it.value().toString());
            url.setQuery(query);
            request->setUrl(url);
        }
    }

    request->setRawHeader("Authorization", generateAuthorizationHeader(oauthParams));

    if (operation == QNetworkAccessManager::PostOperation)
        request->setHeader(QNetworkRequest::ContentTypeHeader,
                           QStringLiteral("application/x-www-form-urlencoded"));
}

QByteArray QOAuth1::nonce()
{
    // https://tools.ietf.org/html/rfc5849#section-3.3
    return QAbstractOAuth::generateRandomString(8);
}

QByteArray QOAuth1::signature(const QVariantMap &parameters,
                              const QUrl &url,
                              QNetworkAccessManager::Operation op,
                              const QString &clientSharedSecret,
                              const QString &tokenSecret)
{
    auto method = static_cast<QOAuth1Signature::HttpRequestMethod>(op);
    QOAuth1Signature signature(url, clientSharedSecret, tokenSecret, method, parameters);
    return signature.hmacSha1().toBase64();
}

QByteArray QOAuth1::generateAuthorizationHeader(const QVariantMap &oauthParams)
{
    // https://tools.ietf.org/html/rfc5849#section-3.5.1
    // TODO Add realm parameter support
    bool first = true;
    QString ret(QStringLiteral("OAuth "));
    QVariantMap headers(oauthParams);
    for (auto it = headers.begin(), end = headers.end(); it != end; ++it) {
        if (first)
            first = false;
        else
            ret += QLatin1String(",");
        ret += it.key() +
               QLatin1String("=\"") +
               QString::fromUtf8(QUrl::toPercentEncoding(it.value().toString())) +
               QLatin1Char('\"');
    }
    return ret.toUtf8();
}

void QOAuth1::grant()
{
    // https://tools.ietf.org/html/rfc5849#section-2
    Q_D(QOAuth1);
    using Key = QOAuth1Private::OAuth1KeyString;

    if (d->temporaryCredentialsUrl.isEmpty()) {
        qWarning("QOAuth1::grant: requestTokenUrl is empty");
        return;
    }
    if (d->tokenCredentialsUrl.isEmpty()) {
        qWarning("QOAuth1::grant: authorizationGrantUrl is empty");
        return;
    }
    if (!d->tokenCredentials.first.isEmpty()) {
        qWarning("QOAuth1::grant: Already authenticated");
        return;
    }

    QMetaObject::Connection connection;
    connection = connect(this, &QAbstractOAuth::statusChanged, [&](Status status) {
        Q_D(QOAuth1);

        if (status == Status::TemporaryCredentialsReceived) {
            if (d->authorizationUrl.isEmpty()) {
                // try upgrading token without verifier
                auto reply = requestTokenCredentials(QNetworkAccessManager::PostOperation,
                                                     d->tokenCredentialsUrl,
                                                     d->tokenCredentials);
                connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
            } else {
                QVariantMap parameters;
                parameters.insert(Key::oauthToken, d->tokenCredentials.first);
                if (d->modifyParametersFunction)
                    d->modifyParametersFunction(Stage::RequestingAuthorization, &parameters);

                // https://tools.ietf.org/html/rfc5849#section-2.2
                resourceOwnerAuthorization(d->authorizationUrl, parameters);
            }
        } else if (status == Status::Granted) {
            Q_EMIT granted();
        } else {
            // Inherit class called QAbstractOAuth::setStatus(Status::NotAuthenticated);
            setTokenCredentials(QString(), QString());
            disconnect(connection);
        }
    });

    auto httpReplyHandler = qobject_cast<QOAuthHttpServerReplyHandler*>(replyHandler());
    if (httpReplyHandler) {
        connect(httpReplyHandler, &QOAuthHttpServerReplyHandler::callbackReceived, [&](
                const QVariantMap &values) {
            QString verifier = values.value(Key::oauthVerifier).toString();
            if (verifier.isEmpty()) {
                qWarning("%s not found in the callback", qPrintable(Key::oauthVerifier));
                return;
            }
            continueGrantWithVerifier(verifier);
        });
    }

    // requesting temporary credentials
    auto reply = requestTemporaryCredentials(QNetworkAccessManager::PostOperation,
                                             d->temporaryCredentialsUrl);
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

void QOAuth1::continueGrantWithVerifier(const QString &verifier)
{
    // https://tools.ietf.org/html/rfc5849#section-2.3
    Q_D(QOAuth1);

    QVariantMap parameters;
    parameters.insert(Key::oauthVerifier, verifier);
    auto reply = requestTokenCredentials(QNetworkAccessManager::PostOperation,
                                         d->tokenCredentialsUrl,
                                         d->tokenCredentials,
                                         parameters);
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

QT_END_NAMESPACE

#endif // QT_NO_HTTP
