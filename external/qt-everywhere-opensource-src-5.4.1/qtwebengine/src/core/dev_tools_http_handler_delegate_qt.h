/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef DEV_TOOLS_HTTP_HANDLER_DELEGATE_QT_H
#define DEV_TOOLS_HTTP_HANDLER_DELEGATE_QT_H

#include "content/public/browser/devtools_http_handler_delegate.h"

#include <QtCore/qcompilerdetection.h> // needed for Q_DECL_OVERRIDE

namespace net {
class StreamListenSocket;
}

namespace content {
class BrowserContext;
class DevToolsHttpHandler;
class RenderViewHost;
}

class DevToolsHttpHandlerDelegateQt : public content::DevToolsHttpHandlerDelegate {
public:

    explicit DevToolsHttpHandlerDelegateQt(content::BrowserContext* browser_context);
    virtual ~DevToolsHttpHandlerDelegateQt();

    // content::DevToolsHttpHandlerDelegate Overrides
    virtual std::string GetDiscoveryPageHTML() Q_DECL_OVERRIDE;
    virtual bool BundlesFrontendResources() Q_DECL_OVERRIDE;
    virtual base::FilePath GetDebugFrontendDir() Q_DECL_OVERRIDE;
    virtual std::string GetPageThumbnailData(const GURL& url) Q_DECL_OVERRIDE;
    virtual scoped_ptr<content::DevToolsTarget> CreateNewTarget(const GURL&) Q_DECL_OVERRIDE;
    // Requests the list of all inspectable targets.
    // The caller gets the ownership of the returned targets.
    virtual void EnumerateTargets(TargetCallback callback) Q_DECL_OVERRIDE;
    virtual scoped_ptr<net::StreamListenSocket> CreateSocketForTethering(net::StreamListenSocket::Delegate* delegate, std::string* name) Q_DECL_OVERRIDE;

private:
    content::BrowserContext* m_browserContext;
    content::DevToolsHttpHandler* m_devtoolsHttpHandler;
};

#endif // DEV_TOOLS_HTTP_HANDLER_DELEGATE_QT_H
