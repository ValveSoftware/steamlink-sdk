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

#ifndef WEB_ENGINE_SETTINGS_H
#define WEB_ENGINE_SETTINGS_H

#include "qtwebenginecoreglobal.h"

#include <QScopedPointer>
#include <QHash>
#include <QUrl>
#include <QSet>

namespace content {
struct WebPreferences;
}
namespace QtWebEngineCore {

class BatchTimer;
class WebContentsAdapter;

class QWEBENGINE_EXPORT WebEngineSettings {
public:
    // Attributes. Names match the ones from the public widgets API.
    enum Attribute {
        UnsupportedInCoreSettings = -1,
        AutoLoadImages,
        JavascriptEnabled,
        JavascriptCanOpenWindows,
        JavascriptCanAccessClipboard,
        LinksIncludedInFocusChain,
        LocalStorageEnabled,
        LocalContentCanAccessRemoteUrls,
        XSSAuditingEnabled,
        SpatialNavigationEnabled,
        LocalContentCanAccessFileUrls,
        HyperlinkAuditingEnabled,
        ScrollAnimatorEnabled,
        ErrorPageEnabled,
        PluginsEnabled,
        FullScreenSupportEnabled,
        ScreenCaptureEnabled,
        WebGLEnabled,
        Accelerated2dCanvasEnabled,
        AutoLoadIconsForPage,
        TouchIconsEnabled,
        FocusOnNavigationEnabled,
        PrintElementBackgrounds,
        AllowRunningInsecureContent
    };

    // Must match the values from the public API in qwebenginesettings.h.
    enum FontFamily {
        StandardFont,
        FixedFont,
        SerifFont,
        SansSerifFont,
        CursiveFont,
        FantasyFont,
        PictographFont
    };

    // Must match the values from the public API in qwebenginesettings.h.
    enum FontSize {
        MinimumFontSize,
        MinimumLogicalFontSize,
        DefaultFontSize,
        DefaultFixedFontSize
    };

    explicit WebEngineSettings(WebEngineSettings *parentSettings = 0);
    ~WebEngineSettings();

    void setParentSettings(WebEngineSettings *parentSettings);

    void overrideWebPreferences(content::WebPreferences *prefs);

    void setAttribute(Attribute, bool on);
    bool testAttribute(Attribute) const;
    void resetAttribute(Attribute);

    void setFontFamily(FontFamily, const QString &);
    QString fontFamily(FontFamily);
    void resetFontFamily(FontFamily);

    void setFontSize(FontSize type, int size);
    int fontSize(FontSize type) const;
    void resetFontSize(FontSize type);

    void setDefaultTextEncoding(const QString &encoding);
    QString defaultTextEncoding() const;

    void initDefaults(bool offTheRecord = false);
    void scheduleApply();

    void scheduleApplyRecursively();

private:
    void doApply();
    void applySettingsToWebPreferences(content::WebPreferences *);
    void setWebContentsAdapter(WebContentsAdapter *adapter) { m_adapter = adapter; }

    WebContentsAdapter* m_adapter;
    QHash<Attribute, bool> m_attributes;
    QHash<FontFamily, QString> m_fontFamilies;
    QHash<FontSize, int> m_fontSizes;
    QString m_defaultEncoding;
    QScopedPointer<content::WebPreferences> webPreferences;
    QScopedPointer<BatchTimer> m_batchTimer;

    WebEngineSettings *parentSettings;
    QSet<WebEngineSettings *> childSettings;

    static QHash<Attribute, bool> s_defaultAttributes;
    static QHash<FontFamily, QString> s_defaultFontFamilies;
    static QHash<FontSize, int> s_defaultFontSizes;

    friend class BatchTimer;
    friend class WebContentsAdapter;
};

} // namespace QtWebEngineCore

#endif // WEB_ENGINE_SETTINGS_H
