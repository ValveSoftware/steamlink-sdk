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

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dev_tools_http_handler_delegate_qt.h"

#include "type_conversion.h"

#include <QByteArray>
#include <QFile>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/devtools_discovery/devtools_discovery_manager.h"
#include "components/devtools_discovery/devtools_target_descriptor.h"
#include "components/devtools_http_handler/devtools_http_handler.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_server_socket.h"

using namespace content;
using namespace devtools_discovery;
using namespace devtools_http_handler;

namespace {

const char kTargetTypePage[] = "page";
const char kTargetTypeSharedWorker[] = "worker";
const char kTargetTypeServiceWorker[] = "service_worker";
const char kTargetTypeOther[] = "other";

class TCPServerSocketFactory
    : public DevToolsHttpHandler::ServerSocketFactory {
public:
    TCPServerSocketFactory(const std::string& address, int port, int backlog)
        : m_address(address), m_port(port), m_backlog(backlog)
    {}
private:
    std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
        std::unique_ptr<net::ServerSocket> socket(new net::TCPServerSocket(nullptr, net::NetLog::Source()));
        if (socket->ListenWithAddressAndPort(m_address, m_port, m_backlog) != net::OK)
            return std::unique_ptr<net::ServerSocket>();

        return socket;
    }

    const std::string m_address;
    int m_port;
    int m_backlog;
    DISALLOW_COPY_AND_ASSIGN(TCPServerSocketFactory);
};

class DevToolsDiscoveryProviderQt : public DevToolsDiscoveryManager::Provider {
public:
    DevToolsTargetDescriptor::List GetDescriptors() override;
};

class Target : public DevToolsTargetDescriptor {
public:
    explicit Target(scoped_refptr<DevToolsAgentHost> agent_host);

    virtual std::string GetId() const override { return agent_host_->GetId(); }
    virtual std::string GetParentId() const override { return std::string(); }
    virtual std::string GetType() const override {
        switch (agent_host_->GetType()) {
        case DevToolsAgentHost::TYPE_WEB_CONTENTS:
            return kTargetTypePage;
        case DevToolsAgentHost::TYPE_SHARED_WORKER:
            return kTargetTypeSharedWorker;
        case DevToolsAgentHost::TYPE_SERVICE_WORKER:
            return kTargetTypeServiceWorker;
        default:
            break;
        }
        return kTargetTypeOther;
    }
    virtual std::string GetTitle() const override { return agent_host_->GetTitle(); }
    virtual std::string GetDescription() const override { return std::string(); }
    virtual GURL GetURL() const override { return agent_host_->GetURL(); }
    virtual GURL GetFaviconURL() const override { return favicon_url_; }
    virtual base::TimeTicks GetLastActivityTime() const override {
        return last_activity_time_;
    }
    virtual bool IsAttached() const override {
        return agent_host_->IsAttached();
    }
    virtual scoped_refptr<DevToolsAgentHost> GetAgentHost() const override {
        return agent_host_;
    }
    virtual bool Activate() const override;
    virtual bool Close() const override;

private:
    scoped_refptr<DevToolsAgentHost> agent_host_;
    GURL favicon_url_;
    base::TimeTicks last_activity_time_;
};

Target::Target(scoped_refptr<DevToolsAgentHost> agent_host)
    : agent_host_(agent_host)
{
    if (WebContents* web_contents = agent_host_->GetWebContents()) {
        NavigationController& controller = web_contents->GetController();
        NavigationEntry* entry = controller.GetActiveEntry();
        if (entry != NULL && entry->GetURL().is_valid())
            favicon_url_ = entry->GetFavicon().url;
        last_activity_time_ = web_contents->GetLastActiveTime();
    }
}

bool Target::Activate() const {
    return agent_host_->Activate();
}

bool Target::Close() const {
    return agent_host_->Close();
}

DevToolsTargetDescriptor::List DevToolsDiscoveryProviderQt::GetDescriptors()
{
    DevToolsTargetDescriptor::List targets;
    for (const auto& agent_host : DevToolsAgentHost::GetOrCreateAll()) {
        targets.push_back(new Target(agent_host));
    }
    return targets;
}

}  // namespace

namespace QtWebEngineCore {

std::unique_ptr<DevToolsHttpHandler> createDevToolsHttpHandler()
{
    DevToolsHttpHandlerDelegateQt *delegate = new DevToolsHttpHandlerDelegateQt();
    if (!delegate->isValid()) {
        delete delegate;
        return nullptr;
    }
    std::unique_ptr<DevToolsHttpHandler::ServerSocketFactory> factory(new TCPServerSocketFactory(delegate->bindAddress().toStdString(), delegate->port(), 1));
    // Ownership of the delegate is taken over the devtools http handler.
    std::unique_ptr<DevToolsHttpHandler> handler(new DevToolsHttpHandler(std::move(factory), std::string(), delegate, base::FilePath(), base::FilePath(), std::string(), std::string()));
    DevToolsDiscoveryManager::GetInstance()->AddProvider(base::WrapUnique(new DevToolsDiscoveryProviderQt()));
    return std::move(handler);
}

DevToolsHttpHandlerDelegateQt::DevToolsHttpHandlerDelegateQt()
    : m_bindAddress(QLatin1String("127.0.0.1"))
    , m_port(0)
    , m_valid(false)
{
    const QString inspectorEnv = QString::fromUtf8(qgetenv("QTWEBENGINE_REMOTE_DEBUGGING"));
    const base::CommandLine &commandLine = *base::CommandLine::ForCurrentProcess();
    QString portStr;

    if (commandLine.HasSwitch(switches::kRemoteDebuggingPort)) {
        portStr = QString::fromStdString(commandLine.GetSwitchValueASCII(switches::kRemoteDebuggingPort));
    } else if (!inspectorEnv.isEmpty()) {
        int portColonPos = inspectorEnv.lastIndexOf(':');
        if (portColonPos != -1) {
            portStr = inspectorEnv.mid(portColonPos + 1);
            m_bindAddress = inspectorEnv.mid(0, portColonPos);
        } else
            portStr = inspectorEnv;
    } else
        return;

    m_port = portStr.toInt(&m_valid);
    m_valid = m_valid && m_port > 0 && m_port < 65535;
    if (!m_valid)
        qWarning("Invalid port given for the inspector server \"%s\". Examples of valid input: \"12345\" or \"192.168.2.14:12345\" (with the address of one of this host's network interface).", qPrintable(portStr));
}

void DevToolsHttpHandlerDelegateQt::Initialized(const net::IPEndPoint *ip_address)
{
    if (ip_address && ip_address->address().size()) {
        QString addressAndPort = QString::fromStdString(ip_address->ToString());
        qWarning("Remote debugging server started successfully. Try pointing a Chromium-based browser to http://%s", qPrintable(addressAndPort));
    } else
        qWarning("Couldn't start the inspector server on bind address \"%s\" and port \"%d\". In case of invalid input, try something like: \"12345\" or \"192.168.2.14:12345\" (with the address of one of this host's interface).", qPrintable(m_bindAddress), m_port);
}

std::string DevToolsHttpHandlerDelegateQt::GetDiscoveryPageHTML()
{
    static std::string html;
    if (html.empty()) {
        QFile html_file(":/data/discovery_page.html");
        html_file.open(QIODevice::ReadOnly);
        QByteArray contents = html_file.readAll();
        html = contents.data();
    }
    return html;
}

std::string DevToolsHttpHandlerDelegateQt::GetPageThumbnailData(const GURL& url)
{
    return std::string();
}

std::string DevToolsHttpHandlerDelegateQt::GetFrontendResource(const std::string &path)
{
    return content::DevToolsFrontendHost::GetFrontendResource(path).as_string();
}

content::DevToolsExternalAgentProxyDelegate* DevToolsHttpHandlerDelegateQt::HandleWebSocketConnection(const std::string&)
{
    return 0;
}

base::DictionaryValue* DevToolsManagerDelegateQt::HandleCommand(DevToolsAgentHost *, base::DictionaryValue *)
{
    return 0;
}

} //namespace QtWebEngineCore
