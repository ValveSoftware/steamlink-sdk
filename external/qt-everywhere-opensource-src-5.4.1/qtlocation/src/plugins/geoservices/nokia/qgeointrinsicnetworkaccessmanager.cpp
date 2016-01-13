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

#include "qgeointrinsicnetworkaccessmanager.h"

#include <QDebug>
#include <QVariant>
#include <QNetworkProxy>
#include <QNetworkAccessManager>

QT_BEGIN_NAMESPACE

QGeoIntrinsicNetworkAccessManager::QGeoIntrinsicNetworkAccessManager(QObject *parent)
: QGeoNetworkAccessManager(parent)
, m_networkManager(new QNetworkAccessManager(this))
{
}

QGeoIntrinsicNetworkAccessManager::QGeoIntrinsicNetworkAccessManager(const QVariantMap &parameters, const QString &token, QObject *parent)
: QGeoNetworkAccessManager(parent)
, m_customProxyToken(token)
, m_networkManager(new QNetworkAccessManager(this))
{
    configure(parameters);
}

void QGeoIntrinsicNetworkAccessManager::configure(const QVariantMap &parameters)
{
    QString proxy = parameters.value(QStringLiteral("proxy")).toString();
    if (proxy.isEmpty() && !m_customProxyToken.isEmpty())
        proxy = parameters.value(m_customProxyToken).toString();

    if (!proxy.isEmpty()) {
#ifndef QT_NO_NETWORKPROXY
        if (proxy.toLower() != QStringLiteral("system")) {
            QUrl proxyUrl(proxy);
            if (proxyUrl.isValid()) {
                qDebug() << "Setting proxy to " << proxyUrl.toString();
                m_networkManager->setProxy(
                    QNetworkProxy(QNetworkProxy::HttpProxy,
                    proxyUrl.host(),
                    proxyUrl.port(8080),
                    proxyUrl.userName(),
                    proxyUrl.password()));
            }
        } else if (QNetworkProxy::applicationProxy().type() == QNetworkProxy::NoProxy) {
            QNetworkProxyFactory::setUseSystemConfiguration(true);
            qDebug() << "Setting system proxy.";
        }
#else
        qDebug() << "No proxy support";
#endif
    } else {
        qDebug() << "No proxy parameter specified.";
    }
}

QNetworkReply *QGeoIntrinsicNetworkAccessManager::get(const QNetworkRequest &request)
{
    return m_networkManager->get(request);
}

QNetworkReply *QGeoIntrinsicNetworkAccessManager::post(const QNetworkRequest &request, const QByteArray &data)
{
    return m_networkManager->post(request, data);
}
QT_END_NAMESPACE
