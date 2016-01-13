/*
 * Copyright © 2013, Jolla.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#include "connmannetworkproxyfactory.h"

#include "networkmanager.h"

ConnmanNetworkProxyFactory::ConnmanNetworkProxyFactory(QObject *parent)
    : QObject(parent)
{
    // Despite its name, createInstance() does not create a new instance every time it is called
    connect(NetworkManagerFactory::createInstance(), SIGNAL(defaultRouteChanged(NetworkService*)),
            this, SLOT(onDefaultRouteChanged(NetworkService*)));
    onDefaultRouteChanged(NetworkManagerFactory::createInstance()->defaultRoute());
}

QList<QNetworkProxy> ConnmanNetworkProxyFactory::queryProxy(const QNetworkProxyQuery & query)
{
    return (query.queryType() == QNetworkProxyQuery::UdpSocket
            || query.queryType() == QNetworkProxyQuery::TcpServer)
        ? m_cachedProxies_udpSocketOrTcpServerCapable
        : m_cachedProxies_all;
}

void ConnmanNetworkProxyFactory::onDefaultRouteChanged(NetworkService *defaultRoute)
{
    if (m_defaultRoute != 0) {
        m_defaultRoute->disconnect(this);
        m_defaultRoute = 0;
    }

    m_cachedProxies_all = QList<QNetworkProxy>() << QNetworkProxy::NoProxy;
    m_cachedProxies_udpSocketOrTcpServerCapable = QList<QNetworkProxy>() << QNetworkProxy::NoProxy;

    if (defaultRoute != 0) {
        m_defaultRoute = defaultRoute;
        connect(m_defaultRoute, SIGNAL(proxyChanged(QVariantMap)),
                this, SLOT(onProxyChanged(QVariantMap)));
        onProxyChanged(m_defaultRoute->proxy());
    }
}

void ConnmanNetworkProxyFactory::onProxyChanged(const QVariantMap &proxy)
{
    m_cachedProxies_all.clear();
    m_cachedProxies_udpSocketOrTcpServerCapable.clear();

    QList<QUrl> proxyUrls;
    if (proxy.value("Method").toString() == QLatin1String("auto")) {
        const QUrl proxyUrl = proxy.value("URL").toUrl();
        if (!proxyUrl.isEmpty()) {
            proxyUrls.append(proxyUrl);
        }
    } else if (proxy.value("Method").toString() == QLatin1String("manual")) {
        const QStringList proxyUrlStrings = proxy.value("Servers").toStringList();
        Q_FOREACH (const QString &proxyUrlString, proxyUrlStrings) {
            proxyUrls.append(QUrl(proxyUrlString));
        }
    }

    Q_FOREACH (const QUrl &url, proxyUrls) {
        if (url.scheme() == QLatin1String("socks5")) {
            QNetworkProxy proxy(QNetworkProxy::Socks5Proxy, url.host(),
                    url.port() ? url.port() : 1080, url.userName(), url.password());
            m_cachedProxies_all.append(proxy);
            m_cachedProxies_udpSocketOrTcpServerCapable.append(proxy);
        } else if (url.scheme() == QLatin1String("socks5h")) {
            QNetworkProxy proxy(QNetworkProxy::Socks5Proxy, url.host(),
                    url.port() ? url.port() : 1080, url.userName(), url.password());
            proxy.setCapabilities(QNetworkProxy::HostNameLookupCapability);
            m_cachedProxies_all.append(proxy);
            m_cachedProxies_udpSocketOrTcpServerCapable.append(proxy);
        } else if (url.scheme() == QLatin1String("http") || url.scheme().isEmpty()) {
            QNetworkProxy proxy(QNetworkProxy::HttpProxy, url.host(),
                    url.port() ? url.port() : 8080, url.userName(), url.password());
            m_cachedProxies_all.append(proxy);
        }
    }

    if (m_cachedProxies_all.isEmpty()) {
        m_cachedProxies_all.append(QNetworkProxy::NoProxy);
    }

    if (m_cachedProxies_udpSocketOrTcpServerCapable.isEmpty()) {
        m_cachedProxies_udpSocketOrTcpServerCapable.append(QNetworkProxy::NoProxy);
    }
}
