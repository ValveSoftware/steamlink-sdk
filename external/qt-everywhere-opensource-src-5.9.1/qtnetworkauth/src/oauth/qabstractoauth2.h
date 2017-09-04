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

#ifndef QABSTRACTOAUTH2_H
#define QABSTRACTOAUTH2_H

#ifndef QT_NO_HTTP

#include <QtCore/qdatetime.h>

#include <QtNetworkAuth/qoauthglobal.h>
#include <QtNetworkAuth/qabstractoauth.h>

QT_BEGIN_NAMESPACE

class QAbstractOAuth2Private;
class Q_OAUTH_EXPORT QAbstractOAuth2 : public QAbstractOAuth
{
    Q_OBJECT
    Q_PROPERTY(QString scope READ scope WRITE setScope NOTIFY scopeChanged)
    Q_PROPERTY(QString userAgent READ userAgent WRITE setUserAgent NOTIFY userAgentChanged)
    Q_PROPERTY(QString clientIdentifier
               READ clientIdentifier
               WRITE setClientIdentifier
               NOTIFY clientIdentifierChanged)
    Q_PROPERTY(QString clientIdentifierSharedKey
               READ clientIdentifierSharedKey
               WRITE setClientIdentifierSharedKey
               NOTIFY clientIdentifierSharedKeyChanged)
    Q_PROPERTY(QString state READ state WRITE setState NOTIFY stateChanged)
    Q_PROPERTY(QDateTime expiration READ expirationAt NOTIFY expirationAtChanged)

public:
    explicit QAbstractOAuth2(QObject *parent = nullptr);
    explicit QAbstractOAuth2(QNetworkAccessManager *manager, QObject *parent = nullptr);
    ~QAbstractOAuth2();

    Q_INVOKABLE virtual QUrl createAuthenticatedUrl(const QUrl &url,
                                                    const QVariantMap &parameters = QVariantMap());
    Q_INVOKABLE virtual QNetworkReply *head(const QUrl &url,
                                const QVariantMap &parameters = QVariantMap()) override;
    Q_INVOKABLE virtual QNetworkReply *get(const QUrl &url,
                               const QVariantMap &parameters = QVariantMap()) override;
    Q_INVOKABLE virtual QNetworkReply *post(const QUrl &url,
                                const QVariantMap &parameters = QVariantMap()) override;
    Q_INVOKABLE virtual QNetworkReply *deleteResource(const QUrl &url,
                                          const QVariantMap &parameters = QVariantMap()) override;

    QString scope() const;
    void setScope(const QString &scope);

    QString userAgent() const;
    void setUserAgent(const QString &userAgent);

    virtual QString responseType() const = 0;

    QString clientIdentifier() const override;
    void setClientIdentifier(const QString &clientIdentifier) override;
    QString clientIdentifierSharedKey() const;
    void setClientIdentifierSharedKey(const QString &clientIdentifierSharedKey);

    QString token() const override;
    void setToken(const QString &token) override;

    QString state() const;
    void setState(const QString &state);

    QDateTime expirationAt() const;

Q_SIGNALS:
    void scopeChanged(const QString &scope);
    void userAgentChanged(const QString &userAgent);
    void clientIdentifierChanged(const QString &clientIdentifier);
    void clientIdentifierSharedKeyChanged(const QString &clientIdentifierSharedKey);
    void stateChanged(const QString &state);
    void expirationAtChanged(const QDateTime &expiration);

    void error(const QString &error, const QString &errorDescription, const QUrl &uri);
    void authorizationCallbackReceived(const QVariantMap &data);

protected:
    explicit QAbstractOAuth2(QAbstractOAuth2Private &, QObject *parent = nullptr);

private:
    Q_DECLARE_PRIVATE(QAbstractOAuth2)
};

QT_END_NAMESPACE

#endif // QT_NO_HTTP

#endif // QABSTRACTOAUTH2_H
