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

#include "qquickwebenginesettings_p.h"

#include "web_engine_settings.h"

#include <QtWebEngine/QQuickWebEngineProfile>
#include <QtCore/QList>

QT_BEGIN_NAMESPACE

using QtWebEngineCore::WebEngineSettings;

QQuickWebEngineSettings::QQuickWebEngineSettings(QQuickWebEngineSettings *parentSettings)
    : d_ptr(new WebEngineSettings(parentSettings ? parentSettings->d_ptr.data() : 0))
{ }

/*!
    \qmltype WebEngineSettings
    \instantiates QQuickWebEngineSettings
    \inqmlmodule QtWebEngine
    \since QtWebEngine 1.1
    \brief Allows configuration of browser properties and attributes.

    The WebEngineSettings type can be used to configure browser properties and generic
    attributes, such as JavaScript support, focus behavior, and access to remote content. This type
    is uncreatable, but the default settings for all web engine views can be accessed by using
    the \l [QML] {WebEngine::settings}{WebEngine.settings} property.

    Each web engine view can have individual settings that can be accessed by using the
    \l{WebEngineView::settings}{WebEngineView.settings} property.
*/


QQuickWebEngineSettings::~QQuickWebEngineSettings()
{ }

/*!
    \qmlproperty bool WebEngineSettings::autoLoadImages

    Automatically loads images on web pages.

    Enabled by default.
*/
bool QQuickWebEngineSettings::autoLoadImages() const
{
    return d_ptr->testAttribute(WebEngineSettings::AutoLoadImages);
}

/*!
    \qmlproperty bool WebEngineSettings::javascriptEnabled

    Enables the running of JavaScript programs.

    Enabled by default.
*/
bool QQuickWebEngineSettings::javascriptEnabled() const
{
    return d_ptr->testAttribute(WebEngineSettings::JavascriptEnabled);
}

/*!
    \qmlproperty bool WebEngineSettings::javascriptCanOpenWindows

    Allows JavaScript programs to open popup windows without user interaction.

    Enabled by default.
*/
bool QQuickWebEngineSettings::javascriptCanOpenWindows() const
{
    return d_ptr->testAttribute(WebEngineSettings::JavascriptCanOpenWindows);
}

/*!
    \qmlproperty bool WebEngineSettings::javascriptCanAccessClipboard

    Allows JavaScript programs to read from or write to the clipboard.

    Disabled by default.
*/
bool QQuickWebEngineSettings::javascriptCanAccessClipboard() const
{
    return d_ptr->testAttribute(WebEngineSettings::JavascriptCanAccessClipboard);
}

/*!
    \qmlproperty bool WebEngineSettings::linksIncludedInFocusChain

    Includes hyperlinks in the keyboard focus chain.

    Enabled by default.
*/
bool QQuickWebEngineSettings::linksIncludedInFocusChain() const
{
    return d_ptr->testAttribute(WebEngineSettings::LinksIncludedInFocusChain);
}

/*!
    \qmlproperty bool WebEngineSettings::localStorageEnabled

    Enables support for the HTML 5 local storage feature.

    Enabled by default.
*/
bool QQuickWebEngineSettings::localStorageEnabled() const
{
    return d_ptr->testAttribute(WebEngineSettings::LocalStorageEnabled);
}

/*!
    \qmlproperty bool WebEngineSettings::localContentCanAccessRemoteUrls

    Allows locally loaded documents to access remote URLs.

    Disabled by default.
*/
bool QQuickWebEngineSettings::localContentCanAccessRemoteUrls() const
{
    return d_ptr->testAttribute(WebEngineSettings::LocalContentCanAccessRemoteUrls);
}

/*!
    \qmlproperty bool WebEngineSettings::spatialNavigationEnabled

    Enables the Spatial Navigation feature, which means the ability to navigate between focusable
    elements, such as hyperlinks and form controls, on a web page by using the Left, Right, Up and
    Down arrow keys.

    For example, if a user presses the Right key, heuristics determine whether there is an element
    they might be trying to reach towards the right and which element they probably want.

    Disabled by default.

*/
bool QQuickWebEngineSettings::spatialNavigationEnabled() const
{
    return d_ptr->testAttribute(WebEngineSettings::SpatialNavigationEnabled);
}

/*!
    \qmlproperty bool WebEngineSettings::localContentCanAccessFileUrls

    Allows locally loaded documents to access other local URLs.

    Enabled by default.
*/
bool QQuickWebEngineSettings::localContentCanAccessFileUrls() const
{
    return d_ptr->testAttribute(WebEngineSettings::LocalContentCanAccessFileUrls);
}

/*!
    \qmlproperty bool WebEngineSettings::hyperlinkAuditingEnabled

    Enables support for the \c ping attribute for hyperlinks.

    Disabled by default.
*/
bool QQuickWebEngineSettings::hyperlinkAuditingEnabled() const
{
    return d_ptr->testAttribute(WebEngineSettings::HyperlinkAuditingEnabled);
}

/*!
    \qmlproperty bool WebEngineSettings::errorPageEnabled

    Enables displaying the built-in error pages of Chromium.

    Enabled by default.
*/
bool QQuickWebEngineSettings::errorPageEnabled() const
{
    return d_ptr->testAttribute(WebEngineSettings::ErrorPageEnabled);
}

/*!
    \qmlproperty bool WebEngineSettings::pluginsEnabled

    Enables support for Pepper plugins, such as the Flash player.

    Disabled by default.

   \sa {Pepper Plugin API}
*/
bool QQuickWebEngineSettings::pluginsEnabled() const
{
    return d_ptr->testAttribute(WebEngineSettings::PluginsEnabled);
}

/*!
    \qmlproperty bool WebEngineSettings::fullscreenSupportEnabled
    \since QtWebEngine 1.2

    Tells the web engine whether fullscreen is supported in this application or not.

    Enabled by default.
*/
bool QQuickWebEngineSettings::fullScreenSupportEnabled() const
{
    return d_ptr->testAttribute(WebEngineSettings::FullScreenSupportEnabled);
}

/*!
    \qmlproperty bool WebEngineSettings::screenCaptureEnabled
    \since QtWebEngine 1.3

    Tells the web engine whether screen capture is supported in this application or not.

    Disabled by default.
*/
bool QQuickWebEngineSettings::screenCaptureEnabled() const
{
    return d_ptr->testAttribute(WebEngineSettings::ScreenCaptureEnabled);
}

/*!
    \qmlproperty bool WebEngineSettings::webGLEnabled
    \since QtWebEngine 1.3

    Enables support for HTML 5 WebGL.

    Enabled by default if available.
*/
bool QQuickWebEngineSettings::webGLEnabled() const
{
    return d_ptr->testAttribute(WebEngineSettings::WebGLEnabled);
}

/*!
    \qmlproperty bool WebEngineSettings::accelerated2dCanvasEnabled
    \since QtWebEngine 1.3

    Specifies whether the HTML 5 2D canvas should be an OpenGL framebuffer.
    This makes many painting operations faster, but slows down pixel access.

    Enabled by default if available.
*/
bool QQuickWebEngineSettings::accelerated2dCanvasEnabled() const
{
    return d_ptr->testAttribute(WebEngineSettings::Accelerated2dCanvasEnabled);
}

/*!
  \qmlproperty bool WebEngineSettings::autoLoadIconsForPage
  \since QtWebEngine 1.3

  Automatically downloads icons for web pages.

  Enabled by default.
*/
bool QQuickWebEngineSettings::autoLoadIconsForPage() const
{
    return d_ptr->testAttribute(WebEngineSettings::AutoLoadIconsForPage);
}

/*!
  \qmlproperty bool WebEngineSettings::touchIconsEnabled
  \since QtWebEngine 1.3

  Enables support for touch icons and precomposed touch icons.

  Disabled by default.
*/
bool QQuickWebEngineSettings::touchIconsEnabled() const
{
    return d_ptr->testAttribute(WebEngineSettings::TouchIconsEnabled);
}

/*!
  \qmlproperty bool WebEngineSettings::focusOnNavigationEnabled
  \since QtWebEngine 1.4

  Focus is given to the view whenever a navigation operation occurs
  (load, stop, reload, reload and bypass cache, forward, backward, set content, and so on).

  Enabled by default. See \l{WebEngine Recipe Browser} for an example where
  this property is disabled.
*/
bool QQuickWebEngineSettings::focusOnNavigationEnabled() const
{
    return d_ptr->testAttribute(WebEngineSettings::FocusOnNavigationEnabled);
}

/*!
  \qmlproperty bool WebEngineSettings::printElementBackgrounds
  \since QtWebEngine 1.4

  Turns on printing of CSS backgrounds when printing a web page.

  Enabled by default.
*/
bool QQuickWebEngineSettings::printElementBackgrounds() const
{
    return d_ptr->testAttribute(WebEngineSettings::PrintElementBackgrounds);
}

/*!
  \qmlproperty bool WebEngineSettings::allowRunningInsecureContent
  \since QtWebEngine 1.4

  By default, HTTPS pages cannot run JavaScript, CSS, plugins or
  web-sockets from HTTP URLs. This used to be possible and this
  provides an override to get the old behavior.

  Disabled by default.
*/
bool QQuickWebEngineSettings::allowRunningInsecureContent() const
{
    return d_ptr->testAttribute(WebEngineSettings::AllowRunningInsecureContent);
}

/*!
    \qmlproperty string WebEngineSettings::defaultTextEncoding
    \since QtWebEngine 1.2

    Sets the default encoding. The value must be a string describing an encoding such as "utf-8" or
    "iso-8859-1".

    If left empty, a default value will be used.
*/
QString QQuickWebEngineSettings::defaultTextEncoding() const
{
    return d_ptr->defaultTextEncoding();
}

void QQuickWebEngineSettings::setAutoLoadImages(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::AutoLoadImages);
    // Set unconditionally as it sets the override for the current settings while the current setting
    // could be from the fallback and is prone to changing later on.
    d_ptr->setAttribute(WebEngineSettings::AutoLoadImages, on);
    if (wasOn != on)
        Q_EMIT autoLoadImagesChanged();
}

void QQuickWebEngineSettings::setJavascriptEnabled(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::JavascriptEnabled);
    d_ptr->setAttribute(WebEngineSettings::JavascriptEnabled, on);
    if (wasOn != on)
        Q_EMIT javascriptEnabledChanged();
}

void QQuickWebEngineSettings::setJavascriptCanOpenWindows(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::JavascriptCanOpenWindows);
    d_ptr->setAttribute(WebEngineSettings::JavascriptCanOpenWindows, on);
    if (wasOn != on)
        Q_EMIT javascriptCanOpenWindowsChanged();
}

void QQuickWebEngineSettings::setJavascriptCanAccessClipboard(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::JavascriptCanAccessClipboard);
    d_ptr->setAttribute(WebEngineSettings::JavascriptCanAccessClipboard, on);
    if (wasOn != on)
        Q_EMIT javascriptCanAccessClipboardChanged();
}

void QQuickWebEngineSettings::setLinksIncludedInFocusChain(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::LinksIncludedInFocusChain);
    d_ptr->setAttribute(WebEngineSettings::LinksIncludedInFocusChain, on);
    if (wasOn != on)
        Q_EMIT linksIncludedInFocusChainChanged();
}

void QQuickWebEngineSettings::setLocalStorageEnabled(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::LocalStorageEnabled);
    d_ptr->setAttribute(WebEngineSettings::LocalStorageEnabled, on);
    if (wasOn != on)
        Q_EMIT localStorageEnabledChanged();
}

void QQuickWebEngineSettings::setLocalContentCanAccessRemoteUrls(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::LocalContentCanAccessRemoteUrls);
    d_ptr->setAttribute(WebEngineSettings::LocalContentCanAccessRemoteUrls, on);
    if (wasOn != on)
        Q_EMIT localContentCanAccessRemoteUrlsChanged();
}


void QQuickWebEngineSettings::setSpatialNavigationEnabled(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::SpatialNavigationEnabled);
    d_ptr->setAttribute(WebEngineSettings::SpatialNavigationEnabled, on);
    if (wasOn != on)
        Q_EMIT spatialNavigationEnabledChanged();
}

void QQuickWebEngineSettings::setLocalContentCanAccessFileUrls(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::LocalContentCanAccessFileUrls);
    d_ptr->setAttribute(WebEngineSettings::LocalContentCanAccessFileUrls, on);
    if (wasOn != on)
        Q_EMIT localContentCanAccessFileUrlsChanged();
}

void QQuickWebEngineSettings::setHyperlinkAuditingEnabled(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::HyperlinkAuditingEnabled);
    d_ptr->setAttribute(WebEngineSettings::HyperlinkAuditingEnabled, on);
    if (wasOn != on)
        Q_EMIT hyperlinkAuditingEnabledChanged();
}

void QQuickWebEngineSettings::setErrorPageEnabled(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::ErrorPageEnabled);
    d_ptr->setAttribute(WebEngineSettings::ErrorPageEnabled, on);
    if (wasOn != on)
        Q_EMIT errorPageEnabledChanged();
}

void QQuickWebEngineSettings::setPluginsEnabled(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::PluginsEnabled);
    d_ptr->setAttribute(WebEngineSettings::PluginsEnabled, on);
    if (wasOn != on)
        Q_EMIT pluginsEnabledChanged();
}

void QQuickWebEngineSettings::setFullScreenSupportEnabled(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::FullScreenSupportEnabled);
    d_ptr->setAttribute(WebEngineSettings::FullScreenSupportEnabled, on);
    if (wasOn != on)
        Q_EMIT fullScreenSupportEnabledChanged();
}

void QQuickWebEngineSettings::setScreenCaptureEnabled(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::ScreenCaptureEnabled);
    d_ptr->setAttribute(WebEngineSettings::ScreenCaptureEnabled, on);
    if (wasOn != on)
        Q_EMIT screenCaptureEnabledChanged();
}

void QQuickWebEngineSettings::setWebGLEnabled(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::WebGLEnabled);
    d_ptr->setAttribute(WebEngineSettings::WebGLEnabled, on);
    if (wasOn != on)
        Q_EMIT webGLEnabledChanged();
}

void QQuickWebEngineSettings::setAccelerated2dCanvasEnabled(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::Accelerated2dCanvasEnabled);
    d_ptr->setAttribute(WebEngineSettings::Accelerated2dCanvasEnabled, on);
    if (wasOn != on)
        Q_EMIT accelerated2dCanvasEnabledChanged();
}

void QQuickWebEngineSettings::setAutoLoadIconsForPage(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::AutoLoadIconsForPage);
    d_ptr->setAttribute(WebEngineSettings::AutoLoadIconsForPage, on);
    if (wasOn != on)
        Q_EMIT autoLoadIconsForPageChanged();
}

void QQuickWebEngineSettings::setTouchIconsEnabled(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::TouchIconsEnabled);
    d_ptr->setAttribute(WebEngineSettings::TouchIconsEnabled, on);
    if (wasOn != on)
        Q_EMIT touchIconsEnabledChanged();
}

void QQuickWebEngineSettings::setPrintElementBackgrounds(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::PrintElementBackgrounds);
    d_ptr->setAttribute(WebEngineSettings::PrintElementBackgrounds, on);
    if (wasOn != on)
        Q_EMIT printElementBackgroundsChanged();
}

void QQuickWebEngineSettings::setDefaultTextEncoding(QString encoding)
{
    const QString oldDefaultTextEncoding = d_ptr->defaultTextEncoding();
    d_ptr->setDefaultTextEncoding(encoding);
    if (oldDefaultTextEncoding.compare(encoding))
        Q_EMIT defaultTextEncodingChanged();
}

void QQuickWebEngineSettings::setFocusOnNavigationEnabled(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::FocusOnNavigationEnabled);
    d_ptr->setAttribute(WebEngineSettings::FocusOnNavigationEnabled, on);
    if (wasOn != on)
        Q_EMIT focusOnNavigationEnabledChanged();
}


void QQuickWebEngineSettings::setAllowRunningInsecureContent(bool on)
{
    bool wasOn = d_ptr->testAttribute(WebEngineSettings::AllowRunningInsecureContent);
    d_ptr->setAttribute(WebEngineSettings::AllowRunningInsecureContent, on);
    if (wasOn != on)
        Q_EMIT allowRunningInsecureContentChanged();
}

void QQuickWebEngineSettings::setParentSettings(QQuickWebEngineSettings *parentSettings)
{
    d_ptr->setParentSettings(parentSettings->d_ptr.data());
    d_ptr->scheduleApplyRecursively();
}

QT_END_NAMESPACE
