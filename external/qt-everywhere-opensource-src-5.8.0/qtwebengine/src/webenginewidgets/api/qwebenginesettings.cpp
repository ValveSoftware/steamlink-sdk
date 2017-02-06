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

#include "qwebenginesettings.h"

#include "qwebengineprofile.h"
#include "web_engine_settings.h"

QT_BEGIN_NAMESPACE

using QtWebEngineCore::WebEngineSettings;

static WebEngineSettings::Attribute toWebEngineAttribute(QWebEngineSettings::WebAttribute attribute)
{
    switch (attribute) {
    case QWebEngineSettings::AutoLoadImages:
        return WebEngineSettings::AutoLoadImages;
    case QWebEngineSettings::JavascriptEnabled:
        return WebEngineSettings::JavascriptEnabled;
    case QWebEngineSettings::JavascriptCanOpenWindows:
        return WebEngineSettings::JavascriptCanOpenWindows;
    case QWebEngineSettings::JavascriptCanAccessClipboard:
        return WebEngineSettings::JavascriptCanAccessClipboard;
    case QWebEngineSettings::LinksIncludedInFocusChain:
        return WebEngineSettings::LinksIncludedInFocusChain;
    case QWebEngineSettings::LocalStorageEnabled:
        return WebEngineSettings::LocalStorageEnabled;
    case QWebEngineSettings::LocalContentCanAccessRemoteUrls:
        return WebEngineSettings::LocalContentCanAccessRemoteUrls;
    case QWebEngineSettings::XSSAuditingEnabled:
        return WebEngineSettings::XSSAuditingEnabled;
    case QWebEngineSettings::SpatialNavigationEnabled:
        return WebEngineSettings::SpatialNavigationEnabled;
    case QWebEngineSettings::LocalContentCanAccessFileUrls:
        return WebEngineSettings::LocalContentCanAccessFileUrls;
    case QWebEngineSettings::HyperlinkAuditingEnabled:
        return WebEngineSettings::HyperlinkAuditingEnabled;
    case QWebEngineSettings::ScrollAnimatorEnabled:
        return WebEngineSettings::ScrollAnimatorEnabled;
    case QWebEngineSettings::ErrorPageEnabled:
        return WebEngineSettings::ErrorPageEnabled;
    case QWebEngineSettings::PluginsEnabled:
        return WebEngineSettings::PluginsEnabled;
    case QWebEngineSettings::FullScreenSupportEnabled:
        return WebEngineSettings::FullScreenSupportEnabled;
    case QWebEngineSettings::ScreenCaptureEnabled:
        return WebEngineSettings::ScreenCaptureEnabled;
    case QWebEngineSettings::WebGLEnabled:
        return WebEngineSettings::WebGLEnabled;
    case QWebEngineSettings::Accelerated2dCanvasEnabled:
        return WebEngineSettings::Accelerated2dCanvasEnabled;
    case QWebEngineSettings::AutoLoadIconsForPage:
        return WebEngineSettings::AutoLoadIconsForPage;
    case QWebEngineSettings::TouchIconsEnabled:
        return WebEngineSettings::TouchIconsEnabled;
    case QWebEngineSettings::FocusOnNavigationEnabled:
        return WebEngineSettings::FocusOnNavigationEnabled;
    case QWebEngineSettings::PrintElementBackgrounds:
        return WebEngineSettings::PrintElementBackgrounds;
    case QWebEngineSettings::AllowRunningInsecureContent:
        return WebEngineSettings::AllowRunningInsecureContent;

    default:
        return WebEngineSettings::UnsupportedInCoreSettings;
    }
}

QWebEngineSettings::QWebEngineSettings(QWebEngineSettings *parentSettings)
    : d_ptr(new WebEngineSettings(parentSettings ? parentSettings->d_func() : 0))
{
    Q_D(QWebEngineSettings);
    d->scheduleApplyRecursively();
}

QWebEngineSettings::~QWebEngineSettings()
{
}

#if QT_DEPRECATED_SINCE(5, 5)
QWebEngineSettings *QWebEngineSettings::globalSettings()
{
    return defaultSettings();
}
#endif

/*!
    Returns the default settings for the web engine page.

    \sa globalSettings()
*/
QWebEngineSettings *QWebEngineSettings::defaultSettings()
{
    return QWebEngineProfile::defaultProfile()->settings();
}

ASSERT_ENUMS_MATCH(WebEngineSettings::StandardFont, QWebEngineSettings::StandardFont)
ASSERT_ENUMS_MATCH(WebEngineSettings::FixedFont, QWebEngineSettings::FixedFont)
ASSERT_ENUMS_MATCH(WebEngineSettings::SerifFont, QWebEngineSettings::SerifFont)
ASSERT_ENUMS_MATCH(WebEngineSettings::SansSerifFont, QWebEngineSettings::SansSerifFont)
ASSERT_ENUMS_MATCH(WebEngineSettings::CursiveFont, QWebEngineSettings::CursiveFont)
ASSERT_ENUMS_MATCH(WebEngineSettings::FantasyFont, QWebEngineSettings::FantasyFont)
ASSERT_ENUMS_MATCH(WebEngineSettings::PictographFont, QWebEngineSettings::PictographFont)

void QWebEngineSettings::setFontFamily(QWebEngineSettings::FontFamily which, const QString &family)
{
    Q_D(QWebEngineSettings);
    d->setFontFamily(static_cast<WebEngineSettings::FontFamily>(which), family);
}

QString QWebEngineSettings::fontFamily(QWebEngineSettings::FontFamily which) const
{
    return d_ptr->fontFamily(static_cast<WebEngineSettings::FontFamily>(which));
}

void QWebEngineSettings::resetFontFamily(QWebEngineSettings::FontFamily which)
{
    d_ptr->resetFontFamily(static_cast<WebEngineSettings::FontFamily>(which));
}

ASSERT_ENUMS_MATCH(WebEngineSettings::DefaultFixedFontSize, QWebEngineSettings::DefaultFixedFontSize)
ASSERT_ENUMS_MATCH(WebEngineSettings::DefaultFontSize, QWebEngineSettings::DefaultFontSize)
ASSERT_ENUMS_MATCH(WebEngineSettings::MinimumFontSize, QWebEngineSettings::MinimumFontSize)
ASSERT_ENUMS_MATCH(WebEngineSettings::MinimumLogicalFontSize, QWebEngineSettings::MinimumLogicalFontSize)

void QWebEngineSettings::setFontSize(QWebEngineSettings::FontSize type, int size)
{
    Q_D(QWebEngineSettings);
    d->setFontSize(static_cast<WebEngineSettings::FontSize>(type), size);
}

int QWebEngineSettings::fontSize(QWebEngineSettings::FontSize type) const
{
    Q_D(const QWebEngineSettings);
    return d->fontSize(static_cast<WebEngineSettings::FontSize>(type));
}

void QWebEngineSettings::resetFontSize(QWebEngineSettings::FontSize type)
{
    Q_D(QWebEngineSettings);
    d->resetFontSize(static_cast<WebEngineSettings::FontSize>(type));
}


void QWebEngineSettings::setDefaultTextEncoding(const QString &encoding)
{
    Q_D(QWebEngineSettings);
    d->setDefaultTextEncoding(encoding);
}

QString QWebEngineSettings::defaultTextEncoding() const
{
    Q_D(const QWebEngineSettings);
    return d->defaultTextEncoding();
}

void QWebEngineSettings::setAttribute(QWebEngineSettings::WebAttribute attr, bool on)
{
    Q_D(QWebEngineSettings);
    WebEngineSettings::Attribute webEngineAttribute = toWebEngineAttribute(attr);
    Q_ASSERT(webEngineAttribute != WebEngineSettings::UnsupportedInCoreSettings);
    d->setAttribute(webEngineAttribute, on);
}

bool QWebEngineSettings::testAttribute(QWebEngineSettings::WebAttribute attr) const
{
    Q_D(const QWebEngineSettings);
    WebEngineSettings::Attribute webEngineAttribute = toWebEngineAttribute(attr);
    Q_ASSERT(webEngineAttribute != WebEngineSettings::UnsupportedInCoreSettings);
    return d->testAttribute(webEngineAttribute);
}

void QWebEngineSettings::resetAttribute(QWebEngineSettings::WebAttribute attr)
{
    Q_D(QWebEngineSettings);
    WebEngineSettings::Attribute webEngineAttribute = toWebEngineAttribute(attr);
    Q_ASSERT(webEngineAttribute != WebEngineSettings::UnsupportedInCoreSettings);
    d->resetAttribute(webEngineAttribute);
}

QT_END_NAMESPACE
