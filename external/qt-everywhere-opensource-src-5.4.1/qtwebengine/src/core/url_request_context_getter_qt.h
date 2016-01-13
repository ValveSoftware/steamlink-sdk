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

#ifndef URL_REQUEST_CONTEXT_GETTER_QT_H
#define URL_REQUEST_CONTEXT_GETTER_QT_H

#include "net/url_request/url_request_context_getter.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/url_constants.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_job_factory_impl.h"

#include "qglobal.h"

namespace net {
class HostResolver;
class MappedHostResolver;
class NetworkDelegate;
class ProxyConfigService;
}

class URLRequestContextGetterQt : public net::URLRequestContextGetter {
public:
    explicit URLRequestContextGetterQt(const base::FilePath &, const base::FilePath &, content::ProtocolHandlerMap *protocolHandlers);

    virtual net::URLRequestContext *GetURLRequestContext() Q_DECL_OVERRIDE;
    virtual scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner() const Q_DECL_OVERRIDE;

private:
    virtual ~URLRequestContextGetterQt() {}

    bool m_ignoreCertificateErrors;
    base::FilePath m_dataPath;
    base::FilePath m_cachePath;
    content::ProtocolHandlerMap m_protocolHandlers;

    scoped_ptr<net::ProxyConfigService> m_proxyConfigService;
    scoped_ptr<net::URLRequestContext> m_urlRequestContext;
    scoped_ptr<net::NetworkDelegate> m_networkDelegate;
    scoped_ptr<net::URLRequestContextStorage> m_storage;
    scoped_ptr<net::URLRequestJobFactoryImpl> m_jobFactory;
};

#endif // URL_REQUEST_CONTEXT_GETTER_QT_H
