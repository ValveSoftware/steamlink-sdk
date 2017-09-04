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

#ifndef DEV_TOOLS_HTTP_HANDLER_DELEGATE_QT_H
#define DEV_TOOLS_HTTP_HANDLER_DELEGATE_QT_H

#include "content/public/browser/devtools_manager_delegate.h"

#include <QString>

namespace content {
class DevToolsSocketFactory;
}

namespace QtWebEngineCore {

class DevToolsServerQt {
public:
    DevToolsServerQt();
    ~DevToolsServerQt();

    bool isValid() const { return m_valid; }
    QString bindAddress() const { return m_bindAddress; }
    int port() const { return m_port; }

    void start();
    void stop();
    bool isStarted() const { return m_isStarted; }

private:
    void parseAddressAndPort();
    std::unique_ptr<content::DevToolsSocketFactory> CreateSocketFactory();

    QString m_bindAddress;
    int m_port;
    bool m_valid;
    bool m_isStarted;
};


class DevToolsManagerDelegateQt : public content::DevToolsManagerDelegate {
public:
    std::string GetDiscoveryPageHTML() override;
    std::string GetFrontendResource(const std::string& path) override;
    void Initialized(const net::IPEndPoint *ip_address) override;
};

} // namespace QtWebEngineCore

#endif // DEV_TOOLS_HTTP_HANDLER_DELEGATE_QT_H
