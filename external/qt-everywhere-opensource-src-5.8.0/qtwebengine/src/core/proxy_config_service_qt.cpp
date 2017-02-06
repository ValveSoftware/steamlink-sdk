/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "proxy_config_service_qt.h"

#include "base/bind.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

net::ProxyServer ProxyConfigServiceQt::fromQNetworkProxy(const QNetworkProxy &qtProxy)
{
    net::ProxyServer::Scheme proxyScheme = net::ProxyServer::SCHEME_INVALID;
    switch (qtProxy.type()) {
    case QNetworkProxy::Socks5Proxy:
        proxyScheme = net::ProxyServer::SCHEME_SOCKS5;
        break;
    case QNetworkProxy::HttpProxy:
    case QNetworkProxy::HttpCachingProxy:
    case QNetworkProxy::FtpCachingProxy:
        proxyScheme = net::ProxyServer::SCHEME_HTTP;
        break;
    case QNetworkProxy::NoProxy:
    default:
        proxyScheme = net::ProxyServer::SCHEME_DIRECT;
            break;
    }
    return net::ProxyServer(proxyScheme, net::HostPortPair(qtProxy.hostName().toStdString(), qtProxy.port()));
}

//================ Based on ChromeProxyConfigService =======================

ProxyConfigServiceQt::ProxyConfigServiceQt(std::unique_ptr<ProxyConfigService> baseService)
    : m_baseService(baseService.release()),
      m_registeredObserver(false)
{
}

ProxyConfigServiceQt::~ProxyConfigServiceQt()
{
    if (m_registeredObserver && m_baseService.get())
        m_baseService->RemoveObserver(this);
}

void ProxyConfigServiceQt::AddObserver(net::ProxyConfigService::Observer *observer)
{
    RegisterObserver();
    m_observers.AddObserver(observer);
}

void ProxyConfigServiceQt::RemoveObserver(net::ProxyConfigService::Observer *observer)
{
    m_observers.RemoveObserver(observer);
}

net::ProxyConfigService::ConfigAvailability ProxyConfigServiceQt::GetLatestProxyConfig(net::ProxyConfig *config)
{
    RegisterObserver();

    // Ask the base service if available.
    net::ProxyConfig systemConfig;
    ConfigAvailability systemAvailability = net::ProxyConfigService::CONFIG_UNSET;
    if (m_baseService.get())
        systemAvailability = m_baseService->GetLatestProxyConfig(&systemConfig);

    const QNetworkProxy &qtProxy = QNetworkProxy::applicationProxy();
    if (qtProxy == m_qtApplicationProxy && !m_qtProxyConfig.proxy_rules().empty()) {
        *config = m_qtProxyConfig;
        return CONFIG_VALID;
    }
    m_qtApplicationProxy = qtProxy;
    m_qtProxyConfig = net::ProxyConfig();
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
    if (qtProxy.type() == QNetworkProxy::NoProxy
            && QNetworkProxyFactory::usesSystemConfiguration()) {
        *config = systemConfig;
        return systemAvailability;
    }
#endif

    net::ProxyConfig::ProxyRules qtRules;
    net::ProxyServer server = fromQNetworkProxy(qtProxy);
    switch (qtProxy.type()) {
    case QNetworkProxy::HttpProxy:
    case QNetworkProxy::Socks5Proxy:
        qtRules.type = net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY;
        qtRules.single_proxies.SetSingleProxyServer(server);
        break;
    case QNetworkProxy::HttpCachingProxy:
        qtRules.type = net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
        qtRules.proxies_for_http.SetSingleProxyServer(server);
        break;
    case QNetworkProxy::FtpCachingProxy:
        qtRules.type = net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
        qtRules.proxies_for_ftp.SetSingleProxyServer(server);
        break;
    default:
        qtRules.type = net::ProxyConfig::ProxyRules::TYPE_NO_RULES;
    }

    qtRules.bypass_rules.AddRuleToBypassLocal(); // don't use proxy for connections to localhost
    m_qtProxyConfig.proxy_rules() = qtRules;
    *config = m_qtProxyConfig;
    return CONFIG_VALID;
}

void ProxyConfigServiceQt::OnLazyPoll()
{
    if (m_qtApplicationProxy != QNetworkProxy::applicationProxy()) {
        net::ProxyConfig unusedConfig;
        OnProxyConfigChanged(unusedConfig, CONFIG_VALID);
    }
    if (m_baseService.get())
        m_baseService->OnLazyPoll();
}


void ProxyConfigServiceQt::OnProxyConfigChanged(const net::ProxyConfig &config, ConfigAvailability availability)
{
    Q_UNUSED(config);
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    if (m_qtApplicationProxy != QNetworkProxy::applicationProxy()
            || m_qtApplicationProxy.type() == QNetworkProxy::NoProxy) {
        net::ProxyConfig actual_config;
        availability = GetLatestProxyConfig(&actual_config);
        if (availability == CONFIG_PENDING)
            return;
        FOR_EACH_OBSERVER(net::ProxyConfigService::Observer, m_observers,
                          OnProxyConfigChanged(actual_config, availability));
    }
}

void ProxyConfigServiceQt::RegisterObserver()
{
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (!m_registeredObserver && m_baseService.get()) {
        m_baseService->AddObserver(this);
        m_registeredObserver = true;
    }
}
