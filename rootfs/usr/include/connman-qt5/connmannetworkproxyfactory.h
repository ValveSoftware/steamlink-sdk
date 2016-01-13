/*
 * Copyright © 2013, Jolla.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef CONNMANNETWORKPROXYFACTORY_H
#define CONNMANNETWORKPROXYFACTORY_H

#include <QtCore/QPointer>
#include <QtCore/QVariantMap>
#include <QtNetwork/QNetworkProxyFactory>

class NetworkService;

class ConnmanNetworkProxyFactory : public QObject, public QNetworkProxyFactory
{
    Q_OBJECT

public:
    ConnmanNetworkProxyFactory(QObject *parent = 0);

    // From QNetworkProxyFactory
    QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery & query);

private Q_SLOTS:
    void onDefaultRouteChanged(NetworkService *defaultRoute);
    void onProxyChanged(const QVariantMap &proxy);

private:
    QPointer<NetworkService> m_defaultRoute;
    QList<QNetworkProxy> m_cachedProxies_all;
    QList<QNetworkProxy> m_cachedProxies_udpSocketOrTcpServerCapable;
};

#endif //CONNMANNETWORKPROXYFACTORY_H
