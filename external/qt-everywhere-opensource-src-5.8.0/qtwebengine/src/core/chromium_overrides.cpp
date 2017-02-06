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

#include "chromium_overrides.h"

#include "gl_context_qt.h"
#include "qtwebenginecoreglobal_p.h"
#include "web_contents_view_qt.h"

#include "base/values.h"
#include "content/browser/renderer_host/pepper/pepper_truetype_font_list.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/font_list.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QFontDatabase>
#include <QStringList>

#if defined(USE_X11)
#include "ui/gfx/x/x11_types.h"
#endif

#if defined(USE_AURA) && !defined(USE_OZONE)
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/platform_font.h"
#endif

namespace QtWebEngineCore {
void GetScreenInfoFromNativeWindow(QWindow* window, blink::WebScreenInfo* results)
{
    QScreen* screen = window->screen();

    blink::WebScreenInfo r;
    r.deviceScaleFactor = screen->devicePixelRatio();
    r.depthPerComponent = 8;
    r.depth = screen->depth();
    r.isMonochrome = (r.depth == 1);

    QRect screenGeometry = screen->geometry();
    r.rect = blink::WebRect(screenGeometry.x(), screenGeometry.y(), screenGeometry.width(), screenGeometry.height());
    QRect available = screen->availableGeometry();
    r.availableRect = blink::WebRect(available.x(), available.y(), available.width(), available.height());
    *results = r;
}

} // namespace QtWebEngineCore

#if defined(USE_X11)
XDisplay* GetQtXDisplay()
{
    return static_cast<XDisplay*>(GLContextHelper::getXDisplay());
}
#endif

namespace content {
class WebContentsImpl;
class WebContentsView;
class WebContentsViewDelegate;
class RenderViewHostDelegateView;

WebContentsView* CreateWebContentsView(WebContentsImpl *web_contents,
    WebContentsViewDelegate *,
    RenderViewHostDelegateView **render_view_host_delegate_view)
{
    QtWebEngineCore::WebContentsViewQt* rv = new QtWebEngineCore::WebContentsViewQt(web_contents);
    *render_view_host_delegate_view = rv;
    return rv;
}

// static
void RenderWidgetHostViewBase::GetDefaultScreenInfo(blink::WebScreenInfo* results) {
    QWindow dummy;
    QtWebEngineCore::GetScreenInfoFromNativeWindow(&dummy, results);
}

} // namespace content

#if defined(USE_AURA) && !defined(USE_OZONE)
namespace content {

// content/common/font_list.h
std::unique_ptr<base::ListValue> GetFontList_SlowBlocking()
{
    std::unique_ptr<base::ListValue> font_list(new base::ListValue);

     QFontDatabase database;
     for (auto family : database.families()){
         base::ListValue* font_item = new base::ListValue();
         font_item->Append(new base::StringValue(family.toStdString()));
         font_item->Append(new base::StringValue(family.toStdString()));  // should be localized name.
         // TODO: Support localized family names.
         font_list->Append(font_item);
     }
    return std::move(font_list);
}

#if defined(ENABLE_PLUGINS)
// content/browser/renderer_host/pepper/pepper_truetype_font_list.h
void GetFontFamilies_SlowBlocking(std::vector<std::string> *)
{
    QT_NOT_USED
}

void GetFontsInFamily_SlowBlocking(const std::string &, std::vector<ppapi::proxy::SerializedTrueTypeFontDesc> *)
{
    QT_NOT_USED
}
#endif //defined(ENABLE_PLUGINS)

} // namespace content

namespace ui {

OSExchangeData::Provider* OSExchangeData::CreateProvider()
{
    QT_NOT_USED
    return 0;
}

} // namespace ui

#endif // defined(USE_AURA) && !defined(USE_OZONE)

#if defined(USE_OPENSSL_CERTS)
namespace net {
class SSLPrivateKey { };
class X509Certificate;

std::unique_ptr<SSLPrivateKey> FetchClientCertPrivateKey(X509Certificate* certificate)
{
    return std::unique_ptr<SSLPrivateKey>();
}

}  // namespace net
#endif
