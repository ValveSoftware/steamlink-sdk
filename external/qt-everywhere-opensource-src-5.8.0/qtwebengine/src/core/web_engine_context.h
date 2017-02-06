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

#ifndef WEB_ENGINE_CONTEXT_H
#define WEB_ENGINE_CONTEXT_H

#include "qtwebenginecoreglobal.h"

#include "base/memory/ref_counted.h"
#include "base/values.h"

#include <QSharedPointer>

namespace base {
class RunLoop;
}

namespace content {
class BrowserMainRunner;
class ContentMainRunner;
}

namespace devtools_http_handler {
class DevToolsHttpHandler;
}

#if defined(ENABLE_BASIC_PRINTING)
namespace printing {
class PrintJobManager;
}
#endif // defined(ENABLE_BASIC_PRINTING)

QT_FORWARD_DECLARE_CLASS(QObject)

namespace QtWebEngineCore {

class BrowserContextAdapter;
class ContentMainDelegateQt;
class SurfaceFactoryQt;

class WebEngineContext : public base::RefCounted<WebEngineContext> {
public:
    static scoped_refptr<WebEngineContext> current();

    QSharedPointer<QtWebEngineCore::BrowserContextAdapter> defaultBrowserContext();
    QObject *globalQObject();
#if defined(ENABLE_BASIC_PRINTING)
    printing::PrintJobManager* getPrintJobManager();
#endif // defined(ENABLE_BASIC_PRINTING)
    void destroyBrowserContext();
    void destroy();

private:
    friend class base::RefCounted<WebEngineContext>;
    WebEngineContext();
    ~WebEngineContext();

    std::unique_ptr<base::RunLoop> m_runLoop;
    std::unique_ptr<ContentMainDelegateQt> m_mainDelegate;
    std::unique_ptr<content::ContentMainRunner> m_contentRunner;
    std::unique_ptr<content::BrowserMainRunner> m_browserRunner;
    QObject* m_globalQObject;
    QSharedPointer<QtWebEngineCore::BrowserContextAdapter> m_defaultBrowserContext;
    std::unique_ptr<devtools_http_handler::DevToolsHttpHandler> m_devtools;
#if defined(ENABLE_BASIC_PRINTING)
    std::unique_ptr<printing::PrintJobManager> m_printJobManager;
#endif // defined(ENABLE_BASIC_PRINTING)
};

} // namespace

#endif // WEB_ENGINE_CONTEXT_H
