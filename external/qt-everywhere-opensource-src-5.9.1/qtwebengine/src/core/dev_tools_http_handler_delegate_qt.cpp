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
#include "content/browser/devtools/devtools_http_handler.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/devtools_socket_factory.h"
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

namespace {

class TCPServerSocketFactory : public content::DevToolsSocketFactory {
public:
    TCPServerSocketFactory(const std::string& address, int port, int backlog)
        : m_address(address), m_port(port), m_backlog(backlog)
    {}
private:
    std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
        std::unique_ptr<net::ServerSocket> socket(new net::TCPServerSocket(nullptr, net::NetLogSource()));
        if (socket->ListenWithAddressAndPort(m_address, m_port, m_backlog) != net::OK)
            return std::unique_ptr<net::ServerSocket>();

        return socket;
    }
    std::unique_ptr<net::ServerSocket> CreateForTethering(std::string* out_name) override
    {
          return nullptr;
    }

    const std::string m_address;
    int m_port;
    int m_backlog;
    DISALLOW_COPY_AND_ASSIGN(TCPServerSocketFactory);
};

}  // namespace

namespace QtWebEngineCore {

DevToolsServerQt::DevToolsServerQt()
    : m_bindAddress(QLatin1String("127.0.0.1"))
    , m_port(0)
    , m_valid(false)
    , m_isStarted(false)
{ }

DevToolsServerQt::~DevToolsServerQt()
{
    stop();
}

void DevToolsServerQt::parseAddressAndPort()
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
    m_valid = m_valid && (m_port > 0 && m_port < 65535);
    if (!m_valid)
        qWarning("Invalid port given for the inspector server \"%s\". Examples of valid input: \"12345\" or \"192.168.2.14:12345\" (with the address of one of this host's network interface).", qPrintable(portStr));
}

std::unique_ptr<content::DevToolsSocketFactory> DevToolsServerQt::CreateSocketFactory()
{
    if (!m_valid)
        return nullptr;
    return std::unique_ptr<content::DevToolsSocketFactory>(
        new TCPServerSocketFactory(m_bindAddress.toStdString(), m_port, 1));
}


void DevToolsServerQt::start()
{
    if (m_isStarted)
        return;

    if (!m_valid)
        parseAddressAndPort();

    std::unique_ptr<content::DevToolsSocketFactory> socketFactory = CreateSocketFactory();
    if (!socketFactory)
        return;

    m_isStarted = true;
    DevToolsAgentHost::StartRemoteDebuggingServer(
        std::move(socketFactory), std::string(),
        base::FilePath(), base::FilePath(),
        std::string(), std::string());
}

void DevToolsServerQt::stop()
{
    DevToolsAgentHost::StopRemoteDebuggingServer();
    m_isStarted = false;
}

void DevToolsManagerDelegateQt::Initialized(const net::IPEndPoint *ip_address)
{
    if (ip_address && ip_address->address().size()) {
        QString addressAndPort = QString::fromStdString(ip_address->ToString());
        qWarning("Remote debugging server started successfully. Try pointing a Chromium-based browser to http://%s", qPrintable(addressAndPort));
    }
    else
        qWarning("Couldn't start the inspector server on bind address. In case of invalid input, try something like: \"12345\" or \"192.168.2.14:12345\" (with the address of one of this host's interface).");
}

std::string DevToolsManagerDelegateQt::GetDiscoveryPageHTML()
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

std::string DevToolsManagerDelegateQt::GetFrontendResource(const std::string& path)
{
    return content::DevToolsFrontendHost::GetFrontendResource(path).as_string();
}

} //namespace QtWebEngineCore
