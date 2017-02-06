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

#include "qoauth1signature.h"
#include "qoauth1signature_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qurlquery.h>
#include <QtCore/qmessageauthenticationcode.h>

#include <QtNetwork/qnetworkaccessmanager.h>

#include <functional>
#include <type_traits>

QT_BEGIN_NAMESPACE

static_assert(static_cast<int>(QOAuth1Signature::HttpRequestMethod::Head) ==
              static_cast<int>(QNetworkAccessManager::HeadOperation) &&
              static_cast<int>(QOAuth1Signature::HttpRequestMethod::Get) ==
              static_cast<int>(QNetworkAccessManager::GetOperation) &&
              static_cast<int>(QOAuth1Signature::HttpRequestMethod::Put) ==
              static_cast<int>(QNetworkAccessManager::PutOperation) &&
              static_cast<int>(QOAuth1Signature::HttpRequestMethod::Post) ==
              static_cast<int>(QNetworkAccessManager::PostOperation) &&
              static_cast<int>(QOAuth1Signature::HttpRequestMethod::Delete) ==
              static_cast<int>(QNetworkAccessManager::DeleteOperation),
              "Invalid QOAuth1Signature::HttpRequestMethod enumeration values");

QOAuth1SignaturePrivate QOAuth1SignaturePrivate::shared_null;

QOAuth1SignaturePrivate::QOAuth1SignaturePrivate()
{}

QOAuth1SignaturePrivate::QOAuth1SignaturePrivate(const QUrl &url,
                                                 QOAuth1Signature::HttpRequestMethod method,
                                                 const QVariantMap &parameters,
                                                 const QString &clientSharedKey,
                                                 const QString &tokenSecret) :
    method(method), url(url), clientSharedKey(clientSharedKey), tokenSecret(tokenSecret),
    parameters(parameters)
{}

QByteArray QOAuth1SignaturePrivate::signatureBaseString() const
{
    // https://tools.ietf.org/html/rfc5849#section-3.4.1
    QByteArray base;

    switch (method) {
    case QOAuth1Signature::HttpRequestMethod::Head:
        base.append("HEAD");
        break;
    case QOAuth1Signature::HttpRequestMethod::Get:
        base.append("GET");
        break;
    case QOAuth1Signature::HttpRequestMethod::Put:
        base.append("PUT");
        break;
    case QOAuth1Signature::HttpRequestMethod::Post:
        base.append("POST");
        break;
    case QOAuth1Signature::HttpRequestMethod::Delete:
        base.append("DELETE");
        break;
    default:
        qCritical("QOAuth1Signature: HttpRequestMethod not supported");
    }
    base.append('&');
    base.append(QUrl::toPercentEncoding(url.toString(QUrl::RemoveQuery)) + "&");

    QVariantMap p = parameters;
    {
        const auto queryItems = QUrlQuery(url.query()).queryItems();
        for (auto it = queryItems.begin(), end = queryItems.end(); it != end; ++it)
            p.insert(it->first, it->second);
    }
    base.append(encodeHeaders(p));
    return base;
}

QByteArray QOAuth1SignaturePrivate::secret() const
{
    QByteArray secret;
    secret.append(QUrl::toPercentEncoding(clientSharedKey));
    secret.append('&');
    secret.append(QUrl::toPercentEncoding(tokenSecret));
    return secret;
}

QByteArray QOAuth1SignaturePrivate::parameterString(const QVariantMap &parameters)
{
    QByteArray ret;
    auto previous = parameters.end();
    for (auto it = parameters.begin(), end = parameters.end(); it != end; previous = it++) {
        if (previous != parameters.end()) {
            if (Q_UNLIKELY(previous.key() == it.key()))
                qWarning("QOAuth: duplicated key %s", qPrintable(it.key()));
            ret.append("&");
        }
        ret.append(QUrl::toPercentEncoding(it.key()));
        ret.append("=");
        ret.append(QUrl::toPercentEncoding(it.value().toString()));
    }
    return ret;
}

QByteArray QOAuth1SignaturePrivate::encodeHeaders(const QVariantMap &headers)
{
    return QUrl::toPercentEncoding(QString::fromLatin1(parameterString(headers)));
}

QOAuth1Signature::QOAuth1Signature(const QUrl &url, QOAuth1Signature::HttpRequestMethod method,
                                   const QVariantMap &parameters) :
    d(new QOAuth1SignaturePrivate(url, method, parameters))
{}

QOAuth1Signature::QOAuth1Signature(const QUrl &url, const QString &clientSharedKey,
                                   const QString &tokenSecret, HttpRequestMethod method,
                                   const QVariantMap &parameters) :
    d(new QOAuth1SignaturePrivate(url, method, parameters, clientSharedKey, tokenSecret))
{}

QOAuth1Signature::QOAuth1Signature(const QOAuth1Signature &other) : d(other.d)
{}

QOAuth1Signature::QOAuth1Signature(QOAuth1Signature &&other) : d(other.d)
{
    other.d = &QOAuth1SignaturePrivate::shared_null;
}

QOAuth1Signature::~QOAuth1Signature()
{}

QOAuth1Signature::HttpRequestMethod QOAuth1Signature::httpRequestMethod() const
{
    return d->method;
}

void QOAuth1Signature::setHttpRequestMethod(QOAuth1Signature::HttpRequestMethod method)
{
    d->method = method;
}

QUrl QOAuth1Signature::url() const
{
    return d->url;
}

void QOAuth1Signature::setUrl(const QUrl &url)
{
    d->url = url;
}

QVariantMap QOAuth1Signature::parameters() const
{
    return d->parameters;
}

void QOAuth1Signature::setParameters(const QVariantMap &parameters)
{
    d->parameters = parameters;
}

void QOAuth1Signature::addRequestBody(const QUrlQuery &body)
{
    const auto list = body.queryItems();
    for (auto it = list.begin(), end = list.end(); it != end; ++it)
        d->parameters.insert(it->first, it->second);
}

void QOAuth1Signature::insert(const QString &key, const QVariant &value)
{
    d->parameters.insert(key, value);
}

QList<QString> QOAuth1Signature::keys() const
{
    return d->parameters.uniqueKeys();
}

QVariant QOAuth1Signature::take(const QString &key)
{
    return d->parameters.take(key);
}

QVariant QOAuth1Signature::value(const QString &key, const QVariant &defaultValue) const
{
    return d->parameters.value(key, defaultValue);
}

QString QOAuth1Signature::clientSharedKey() const
{
    return d->clientSharedKey;
}

void QOAuth1Signature::setClientSharedKey(const QString &secret)
{
    d->clientSharedKey = secret;
}

QString QOAuth1Signature::tokenSecret() const
{
    return d->tokenSecret;
}

void QOAuth1Signature::setTokenSecret(const QString &secret)
{
    d->tokenSecret = secret;
}

QByteArray QOAuth1Signature::hmacSha1() const
{
    QMessageAuthenticationCode code(QCryptographicHash::Sha1);
    code.setKey(d->secret());
    code.addData(d->signatureBaseString());
    return code.result();
}

QByteArray QOAuth1Signature::rsaSha1() const
{
    qCritical("QOAuth1Signature::rsaSha1: RSA-SHA1 signing method not supported");
    return QByteArray();
}

QByteArray QOAuth1Signature::plainText(const QString &clientIdentifier) const
{
    return plainText(clientIdentifier, d->clientSharedKey);
}

QByteArray QOAuth1Signature::plainText(const QString &clientIdentifier,
                                       const QString clientSharedKey)
{
    QByteArray ret;
    ret += clientIdentifier.toUtf8() + '&' + clientSharedKey.toUtf8();
    return ret;
}

QByteArray QOAuth1Signature::plainText(const QPair<QString, QString> &clientCredentials)
{
    return plainText(clientCredentials.first, clientCredentials.second);
}

void QOAuth1Signature::swap(QOAuth1Signature &other)
{
    qSwap(d, other.d);
}

QOAuth1Signature &QOAuth1Signature::operator=(const QOAuth1Signature &other)
{
    if (d != other.d) {
        QOAuth1Signature tmp(other);
        tmp.swap(*this);
    }
    return *this;
}

QOAuth1Signature &QOAuth1Signature::operator=(QOAuth1Signature &&other)
{
    QOAuth1Signature moved(std::move(other));
    swap(moved);
    return *this;
}

QT_END_NAMESPACE
