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

#include "web_engine_settings.h"

#include "web_contents_adapter.h"
#include "web_engine_context.h"
#include "type_conversion.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/web_preferences.h"

#include <QFont>
#include <QTimer>
#include <QTouchDevice>

namespace QtWebEngineCore {

QHash<WebEngineSettings::Attribute, bool> WebEngineSettings::s_defaultAttributes;
QHash<WebEngineSettings::FontFamily, QString> WebEngineSettings::s_defaultFontFamilies;
QHash<WebEngineSettings::FontSize, int> WebEngineSettings::s_defaultFontSizes;

static const int batchTimerTimeout = 0;

class BatchTimer : public QTimer {
    Q_OBJECT
public:
    BatchTimer(WebEngineSettings *settings)
        : m_settings(settings)
    {
        setSingleShot(true);
        setInterval(batchTimerTimeout);
        connect(this, SIGNAL(timeout()), SLOT(onTimeout()));
    }

private Q_SLOTS:
    void onTimeout()
    {
        m_settings->doApply();
    }

private:
    WebEngineSettings *m_settings;
};

static inline bool isTouchScreenAvailable() {
    static bool initialized = false;
    static bool touchScreenAvailable = false;
    if (!initialized) {
        Q_FOREACH (const QTouchDevice *d, QTouchDevice::devices()) {
            if (d->type() == QTouchDevice::TouchScreen) {
                touchScreenAvailable = true;
                break;
            }
        }
        initialized = true;
    }
    return touchScreenAvailable;
}


WebEngineSettings::WebEngineSettings(WebEngineSettings *_parentSettings)
    : m_adapter(0)
    , m_batchTimer(new BatchTimer(this))
    , parentSettings(_parentSettings)
{
    if (parentSettings)
        parentSettings->childSettings.insert(this);
}

WebEngineSettings::~WebEngineSettings()
{
    if (parentSettings)
        parentSettings->childSettings.remove(this);
    // In QML the profile and its settings may be garbage collected before the page and its settings.
    Q_FOREACH (WebEngineSettings *settings, childSettings) {
        settings->parentSettings = 0;
    }
}

void WebEngineSettings::overrideWebPreferences(content::WebPreferences *prefs)
{
    // Apply our settings on top of those.
    applySettingsToWebPreferences(prefs);
    // Store the current webPreferences in use if this is the first time we get here
    // as the host process already overides some of the default WebPreferences values
    // before we get here (e.g. number_of_cpu_cores).
    if (webPreferences.isNull())
        webPreferences.reset(new content::WebPreferences(*prefs));
}

void WebEngineSettings::setAttribute(WebEngineSettings::Attribute attr, bool on)
{
    m_attributes.insert(attr, on);
    scheduleApplyRecursively();
}

bool WebEngineSettings::testAttribute(WebEngineSettings::Attribute attr) const
{
    if (!parentSettings) {
        Q_ASSERT(s_defaultAttributes.contains(attr));
        return m_attributes.value(attr, s_defaultAttributes.value(attr));
    }
    return m_attributes.value(attr, parentSettings->testAttribute(attr));
}

void WebEngineSettings::resetAttribute(WebEngineSettings::Attribute attr)
{
    m_attributes.remove(attr);
    scheduleApplyRecursively();
}

void WebEngineSettings::setFontFamily(WebEngineSettings::FontFamily which, const QString &family)
{
    m_fontFamilies.insert(which, family);
    scheduleApplyRecursively();
}

QString WebEngineSettings::fontFamily(WebEngineSettings::FontFamily which)
{
    if (!parentSettings) {
        Q_ASSERT(s_defaultFontFamilies.contains(which));
        return m_fontFamilies.value(which, s_defaultFontFamilies.value(which));
    }
    return m_fontFamilies.value(which, parentSettings->fontFamily(which));
}

void WebEngineSettings::resetFontFamily(WebEngineSettings::FontFamily which)
{
    m_fontFamilies.remove(which);
    scheduleApplyRecursively();
}

void WebEngineSettings::setFontSize(WebEngineSettings::FontSize type, int size)
{
    m_fontSizes.insert(type, size);
    scheduleApplyRecursively();
}

int WebEngineSettings::fontSize(WebEngineSettings::FontSize type) const
{
    if (!parentSettings) {
        Q_ASSERT(s_defaultFontSizes.contains(type));
        return m_fontSizes.value(type, s_defaultFontSizes.value(type));
    }
    return m_fontSizes.value(type, parentSettings->fontSize(type));
}

void WebEngineSettings::resetFontSize(WebEngineSettings::FontSize type)
{
    m_fontSizes.remove(type);
    scheduleApplyRecursively();
}

void WebEngineSettings::setDefaultTextEncoding(const QString &encoding)
{
    m_defaultEncoding = encoding;
    scheduleApplyRecursively();
}

QString WebEngineSettings::defaultTextEncoding() const
{
    if (!parentSettings)
        return m_defaultEncoding;
    return m_defaultEncoding.isEmpty()? parentSettings->defaultTextEncoding() : m_defaultEncoding;
}

void WebEngineSettings::initDefaults(bool offTheRecord)
{
    if (s_defaultAttributes.isEmpty()) {
        // Initialize the default settings.
        s_defaultAttributes.insert(AutoLoadImages, true);
        s_defaultAttributes.insert(JavascriptEnabled, true);
        s_defaultAttributes.insert(JavascriptCanOpenWindows, true);
        s_defaultAttributes.insert(JavascriptCanAccessClipboard, false);
        s_defaultAttributes.insert(LinksIncludedInFocusChain, true);
        s_defaultAttributes.insert(LocalStorageEnabled, true);
        s_defaultAttributes.insert(LocalContentCanAccessRemoteUrls, false);
        s_defaultAttributes.insert(XSSAuditingEnabled, false);
        s_defaultAttributes.insert(SpatialNavigationEnabled, false);
        s_defaultAttributes.insert(LocalContentCanAccessFileUrls, true);
        s_defaultAttributes.insert(HyperlinkAuditingEnabled, false);
        s_defaultAttributes.insert(ErrorPageEnabled, true);
        s_defaultAttributes.insert(PluginsEnabled, false);
        s_defaultAttributes.insert(FullScreenSupportEnabled, false);
        s_defaultAttributes.insert(ScreenCaptureEnabled, false);
        // The following defaults matches logic in render_view_host_impl.cc
        // But first we must ensure the WebContext has been initialized
        QtWebEngineCore::WebEngineContext::current();
        base::CommandLine* commandLine = base::CommandLine::ForCurrentProcess();
        bool smoothScrolling = commandLine->HasSwitch(switches::kEnableSmoothScrolling);
        bool webGL = content::GpuProcessHost::gpu_enabled() &&
                !commandLine->HasSwitch(switches::kDisable3DAPIs) &&
                !commandLine->HasSwitch(switches::kDisableExperimentalWebGL);
        bool accelerated2dCanvas = content::GpuProcessHost::gpu_enabled() &&
                !commandLine->HasSwitch(switches::kDisableAccelerated2dCanvas);
        bool allowRunningInsecureContent = commandLine->HasSwitch(switches::kAllowRunningInsecureContent);
        s_defaultAttributes.insert(ScrollAnimatorEnabled, smoothScrolling);
        s_defaultAttributes.insert(WebGLEnabled, webGL);
        s_defaultAttributes.insert(Accelerated2dCanvasEnabled, accelerated2dCanvas);
        s_defaultAttributes.insert(AutoLoadIconsForPage, true);
        s_defaultAttributes.insert(TouchIconsEnabled, false);
        s_defaultAttributes.insert(FocusOnNavigationEnabled, true);
        s_defaultAttributes.insert(PrintElementBackgrounds, true);
        s_defaultAttributes.insert(AllowRunningInsecureContent, allowRunningInsecureContent);
    }
    if (offTheRecord)
        m_attributes.insert(LocalStorageEnabled, false);

    if (s_defaultFontFamilies.isEmpty()) {
        // Default fonts
        QFont defaultFont;
        defaultFont.setStyleHint(QFont::Serif);
        s_defaultFontFamilies.insert(StandardFont, defaultFont.defaultFamily());
        s_defaultFontFamilies.insert(SerifFont, defaultFont.defaultFamily());
        s_defaultFontFamilies.insert(PictographFont, defaultFont.defaultFamily());

        defaultFont.setStyleHint(QFont::Fantasy);
        s_defaultFontFamilies.insert(FantasyFont, defaultFont.defaultFamily());

        defaultFont.setStyleHint(QFont::Cursive);
        s_defaultFontFamilies.insert(CursiveFont, defaultFont.defaultFamily());

        defaultFont.setStyleHint(QFont::SansSerif);
        s_defaultFontFamilies.insert(SansSerifFont, defaultFont.defaultFamily());

        defaultFont.setStyleHint(QFont::Monospace);
        s_defaultFontFamilies.insert(FixedFont, defaultFont.defaultFamily());
    }

    if (s_defaultFontSizes.isEmpty()) {
        s_defaultFontSizes.insert(MinimumFontSize, 0);
        s_defaultFontSizes.insert(MinimumLogicalFontSize, 6);
        s_defaultFontSizes.insert(DefaultFixedFontSize, 13);
        s_defaultFontSizes.insert(DefaultFontSize, 16);
    }

    m_defaultEncoding = QStringLiteral("ISO-8859-1");
}

void WebEngineSettings::scheduleApply()
{
    if (!m_batchTimer->isActive())
        m_batchTimer->start();
}

void WebEngineSettings::doApply()
{
    if (webPreferences.isNull())
        return;
    // Override with our settings when applicable
    applySettingsToWebPreferences(webPreferences.data());

    Q_ASSERT(m_adapter);
    m_adapter->updateWebPreferences(*webPreferences.data());
}

void WebEngineSettings::applySettingsToWebPreferences(content::WebPreferences *prefs)
{
    // Override for now
    prefs->touch_enabled = isTouchScreenAvailable();

    // Attributes mapping.
    prefs->loads_images_automatically = testAttribute(AutoLoadImages);
    prefs->javascript_enabled = testAttribute(JavascriptEnabled);
    prefs->javascript_can_open_windows_automatically = testAttribute(JavascriptCanOpenWindows);
    prefs->javascript_can_access_clipboard = testAttribute(JavascriptCanAccessClipboard);
    prefs->tabs_to_links = testAttribute(LinksIncludedInFocusChain);
    prefs->local_storage_enabled = testAttribute(LocalStorageEnabled);
    prefs->databases_enabled = testAttribute(LocalStorageEnabled);
    prefs->allow_universal_access_from_file_urls = testAttribute(LocalContentCanAccessRemoteUrls);
    prefs->xss_auditor_enabled = testAttribute(XSSAuditingEnabled);
    prefs->spatial_navigation_enabled = testAttribute(SpatialNavigationEnabled);
    prefs->allow_file_access_from_file_urls = testAttribute(LocalContentCanAccessFileUrls);
    prefs->hyperlink_auditing_enabled = testAttribute(HyperlinkAuditingEnabled);
    prefs->enable_scroll_animator = testAttribute(ScrollAnimatorEnabled);
    prefs->enable_error_page = testAttribute(ErrorPageEnabled);
    prefs->plugins_enabled = testAttribute(PluginsEnabled);
    prefs->fullscreen_supported = testAttribute(FullScreenSupportEnabled);
    prefs->accelerated_2d_canvas_enabled = testAttribute(Accelerated2dCanvasEnabled);
    prefs->experimental_webgl_enabled = testAttribute(WebGLEnabled);
    prefs->should_print_backgrounds = testAttribute(PrintElementBackgrounds);
    prefs->allow_running_insecure_content = testAttribute(AllowRunningInsecureContent);

    // Fonts settings.
    prefs->standard_font_family_map[content::kCommonScript] = toString16(fontFamily(StandardFont));
    prefs->fixed_font_family_map[content::kCommonScript] = toString16(fontFamily(FixedFont));
    prefs->serif_font_family_map[content::kCommonScript] = toString16(fontFamily(SerifFont));
    prefs->sans_serif_font_family_map[content::kCommonScript] = toString16(fontFamily(SansSerifFont));
    prefs->cursive_font_family_map[content::kCommonScript] = toString16(fontFamily(CursiveFont));
    prefs->fantasy_font_family_map[content::kCommonScript] = toString16(fontFamily(FantasyFont));
    prefs->pictograph_font_family_map[content::kCommonScript] = toString16(fontFamily(PictographFont));
    prefs->default_font_size = fontSize(DefaultFontSize);
    prefs->default_fixed_font_size = fontSize(DefaultFixedFontSize);
    prefs->minimum_font_size = fontSize(MinimumFontSize);
    prefs->minimum_logical_font_size = fontSize(MinimumLogicalFontSize);
    prefs->default_encoding = defaultTextEncoding().toStdString();
}

void WebEngineSettings::scheduleApplyRecursively()
{
    scheduleApply();
    Q_FOREACH (WebEngineSettings *settings, childSettings) {
        settings->scheduleApply();
    }
}

void WebEngineSettings::setParentSettings(WebEngineSettings *_parentSettings)
{
    if (parentSettings)
        parentSettings->childSettings.remove(this);
    parentSettings = _parentSettings;
    if (parentSettings)
        parentSettings->childSettings.insert(this);
}

} // namespace QtWebEngineCore

#include "web_engine_settings.moc"
