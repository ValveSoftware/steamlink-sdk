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
#ifndef CONTENT_RENDERER_CLIENT_QT_H
#define CONTENT_RENDERER_CLIENT_QT_H

#include "content/public/renderer/content_renderer_client.h"
#include "components/spellcheck/spellcheck_build_features.h"

#include <QScopedPointer>

namespace visitedlink {
class VisitedLinkSlave;
}

namespace web_cache {
class WebCacheImpl;
}

#if BUILDFLAG(ENABLE_SPELLCHECK)
class SpellCheck;
#endif

namespace QtWebEngineCore {

class ContentRendererClientQt : public content::ContentRendererClient {
public:
    ContentRendererClientQt();
    ~ContentRendererClientQt();
    void RenderThreadStarted() override;
    void RenderViewCreated(content::RenderView *render_view) override;
    void RenderFrameCreated(content::RenderFrame* render_frame) override;
    bool ShouldSuppressErrorPage(content::RenderFrame *, const GURL &) override;
    bool HasErrorPage(int httpStatusCode, std::string *errorDomain) override;
    void GetNavigationErrorStrings(content::RenderFrame* renderFrame, const blink::WebURLRequest& failedRequest,
                                   const blink::WebURLError& error, std::string* errorHtml, base::string16* errorDescription) override;

    unsigned long long VisitedLinkHash(const char *canonicalUrl, size_t length) override;
    bool IsLinkVisited(unsigned long long linkHash) override;
    void AddSupportedKeySystems(std::vector<std::unique_ptr<media::KeySystemProperties>>* key_systems) override;

    void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
    void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) override;

private:
    QScopedPointer<visitedlink::VisitedLinkSlave> m_visitedLinkSlave;
    QScopedPointer<web_cache::WebCacheImpl> m_webCacheImpl;
#if BUILDFLAG(ENABLE_SPELLCHECK)
    QScopedPointer<SpellCheck> m_spellCheck;
#endif
};

} // namespace

#endif // CONTENT_RENDERER_CLIENT_QT_H
