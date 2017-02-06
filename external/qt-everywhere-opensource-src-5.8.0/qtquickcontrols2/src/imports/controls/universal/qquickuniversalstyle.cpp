/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickuniversalstyle_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qsettings.h>
#include <QtQml/qqmlinfo.h>
#include <QtQuickControls2/private/qquickstyleattached_p.h>

QT_BEGIN_NAMESPACE

static QRgb qquickuniversal_light_color(QQuickUniversalStyle::SystemColor role)
{
    static const QRgb colors[] = {
        0xFFFFFFFF, // SystemAltHighColor
        0x33FFFFFF, // SystemAltLowColor
        0x99FFFFFF, // SystemAltMediumColor
        0xCCFFFFFF, // SystemAltMediumHighColor
        0x66FFFFFF, // SystemAltMediumLowColor
        0xFF000000, // SystemBaseHighColor
        0x33000000, // SystemBaseLowColor
        0x99000000, // SystemBaseMediumColor
        0xCC000000, // SystemBaseMediumHighColor
        0x66000000, // SystemBaseMediumLowColor
        0xFF171717, // SystemChromeAltLowColor
        0xFF000000, // SystemChromeBlackHighColor
        0x33000000, // SystemChromeBlackLowColor
        0x66000000, // SystemChromeBlackMediumLowColor
        0xCC000000, // SystemChromeBlackMediumColor
        0xFFCCCCCC, // SystemChromeDisabledHighColor
        0xFF7A7A7A, // SystemChromeDisabledLowColor
        0xFFCCCCCC, // SystemChromeHighColor
        0xFFF2F2F2, // SystemChromeLowColor
        0xFFE6E6E6, // SystemChromeMediumColor
        0xFFF2F2F2, // SystemChromeMediumLowColor
        0xFFFFFFFF, // SystemChromeWhiteColor
        0x19000000, // SystemListLowColor
        0x33000000  // SystemListMediumColor
    };
    return colors[role];
}

static QRgb qquickuniversal_dark_color(QQuickUniversalStyle::SystemColor role)
{
    static const QRgb colors[] = {
        0xFF000000, // SystemAltHighColor
        0x33000000, // SystemAltLowColor
        0x99000000, // SystemAltMediumColor
        0xCC000000, // SystemAltMediumHighColor
        0x66000000, // SystemAltMediumLowColor
        0xFFFFFFFF, // SystemBaseHighColor
        0x33FFFFFF, // SystemBaseLowColor
        0x99FFFFFF, // SystemBaseMediumColor
        0xCCFFFFFF, // SystemBaseMediumHighColor
        0x66FFFFFF, // SystemBaseMediumLowColor
        0xFFF2F2F2, // SystemChromeAltLowColor
        0xFF000000, // SystemChromeBlackHighColor
        0x33000000, // SystemChromeBlackLowColor
        0x66000000, // SystemChromeBlackMediumLowColor
        0xCC000000, // SystemChromeBlackMediumColor
        0xFF333333, // SystemChromeDisabledHighColor
        0xFF858585, // SystemChromeDisabledLowColor
        0xFF767676, // SystemChromeHighColor
        0xFF171717, // SystemChromeLowColor
        0xFF1F1F1F, // SystemChromeMediumColor
        0xFF2B2B2B, // SystemChromeMediumLowColor
        0xFFFFFFFF, // SystemChromeWhiteColor
        0x19FFFFFF, // SystemListLowColor
        0x33FFFFFF  // SystemListMediumColor
    };
    return colors[role];
}

static QRgb qquickuniversal_accent_color(QQuickUniversalStyle::Color accent)
{
    static const QRgb colors[] = {
        0xFFA4C400, // Lime
        0xFF60A917, // Green
        0xFF008A00, // Emerald
        0xFF00ABA9, // Teal
        0xFF1BA1E2, // Cyan
        0xFF3E65FF, // Cobalt
        0xFF6A00FF, // Indigo
        0xFFAA00FF, // Violet
        0xFFF472D0, // Pink
        0xFFD80073, // Magenta
        0xFFA20025, // Crimson
        0xFFE51400, // Red
        0xFFFA6800, // Orange
        0xFFF0A30A, // Amber
        0xFFE3C800, // Yellow
        0xFF825A2C, // Brown
        0xFF6D8764, // Olive
        0xFF647687, // Steel
        0xFF76608A, // Mauve
        0xFF87794E  // Taupe
    };
    return colors[accent];
}

extern bool qt_is_dark_system_theme();

static QQuickUniversalStyle::Theme qquickuniversal_effective_theme(QQuickUniversalStyle::Theme theme)
{
    if (theme == QQuickUniversalStyle::System)
        theme = qt_is_dark_system_theme() ? QQuickUniversalStyle::Dark : QQuickUniversalStyle::Light;
    return theme;
}

// If no value was inherited from a parent or explicitly set, the "global" values are used.
// The initial, default values of the globals are hard-coded here, but the environment
// variables and .conf file override them if specified.
static QQuickUniversalStyle::Theme GlobalTheme = QQuickUniversalStyle::Light;
static QRgb GlobalAccent = qquickuniversal_accent_color(QQuickUniversalStyle::Cobalt);
static QRgb GlobalForeground = qquickuniversal_light_color(QQuickUniversalStyle::BaseHigh);
static QRgb GlobalBackground = qquickuniversal_light_color(QQuickUniversalStyle::AltHigh);
// These represent whether a global foreground/background was set.
// Each style's m_hasForeground/m_hasBackground are initialized to these values.
static bool HasGlobalForeground = false;
static bool HasGlobalBackground = false;

QQuickUniversalStyle::QQuickUniversalStyle(QObject *parent) : QQuickStyleAttached(parent),
    m_explicitTheme(false), m_explicitAccent(false), m_explicitForeground(false), m_explicitBackground(false),
    m_hasForeground(HasGlobalForeground), m_hasBackground(HasGlobalBackground), m_theme(GlobalTheme),
    m_accent(GlobalAccent), m_foreground(GlobalForeground), m_background(GlobalBackground)
{
    init();
}

QQuickUniversalStyle *QQuickUniversalStyle::qmlAttachedProperties(QObject *object)
{
    return new QQuickUniversalStyle(object);
}

QQuickUniversalStyle::Theme QQuickUniversalStyle::theme() const
{
    return m_theme;
}

void QQuickUniversalStyle::setTheme(Theme theme)
{
    theme = qquickuniversal_effective_theme(theme);
    m_explicitTheme = true;
    if (m_theme == theme)
        return;

    m_theme = theme;
    propagateTheme();
    emit themeChanged();
    emit paletteChanged();
    emit foregroundChanged();
    emit backgroundChanged();
}

void QQuickUniversalStyle::inheritTheme(Theme theme)
{
    if (m_explicitTheme || m_theme == theme)
        return;

    m_theme = theme;
    propagateTheme();
    emit themeChanged();
    emit paletteChanged();
    emit foregroundChanged();
    emit backgroundChanged();
}

void QQuickUniversalStyle::propagateTheme()
{
    const auto styles = childStyles();
    for (QQuickStyleAttached *child : styles) {
        QQuickUniversalStyle *universal = qobject_cast<QQuickUniversalStyle *>(child);
        if (universal)
            universal->inheritTheme(m_theme);
    }
}

void QQuickUniversalStyle::resetTheme()
{
    if (!m_explicitTheme)
        return;

    m_explicitTheme = false;
    QQuickUniversalStyle *universal = qobject_cast<QQuickUniversalStyle *>(parentStyle());
    inheritTheme(universal ? universal->theme() : GlobalTheme);
}

QVariant QQuickUniversalStyle::accent() const
{
    return QColor::fromRgba(m_accent);
}

void QQuickUniversalStyle::setAccent(const QVariant &var)
{
    QRgb accent = 0;
    if (!variantToRgba(var, "accent", &accent))
        return;

    m_explicitAccent = true;
    if (m_accent == accent)
        return;

    m_accent = accent;
    propagateAccent();
    emit accentChanged();
}

void QQuickUniversalStyle::inheritAccent(QRgb accent)
{
    if (m_explicitAccent || m_accent == accent)
        return;

    m_accent = accent;
    propagateAccent();
    emit accentChanged();
}

void QQuickUniversalStyle::propagateAccent()
{
    const auto styles = childStyles();
    for (QQuickStyleAttached *child : styles) {
        QQuickUniversalStyle *universal = qobject_cast<QQuickUniversalStyle *>(child);
        if (universal)
            universal->inheritAccent(m_accent);
    }
}

void QQuickUniversalStyle::resetAccent()
{
    if (!m_explicitAccent)
        return;

    m_explicitAccent = false;
    QQuickUniversalStyle *universal = qobject_cast<QQuickUniversalStyle *>(parentStyle());
    inheritAccent(universal ? universal->m_accent : GlobalAccent);
}

QVariant QQuickUniversalStyle::foreground() const
{
    if (m_hasForeground)
        return QColor::fromRgba(m_foreground);
    return baseHighColor();
}

void QQuickUniversalStyle::setForeground(const QVariant &var)
{
    QRgb foreground = 0;
    if (!variantToRgba(var, "foreground", &foreground))
        return;

    m_hasForeground = true;
    m_explicitForeground = true;
    if (m_foreground == foreground)
        return;

    m_foreground = foreground;
    propagateForeground();
    emit foregroundChanged();
}

void QQuickUniversalStyle::inheritForeground(QRgb foreground, bool has)
{
    if (m_explicitForeground || m_foreground == foreground)
        return;

    m_hasForeground = has;
    m_foreground = foreground;
    propagateForeground();
    emit foregroundChanged();
}

void QQuickUniversalStyle::propagateForeground()
{
    const auto styles = childStyles();
    for (QQuickStyleAttached *child : styles) {
        QQuickUniversalStyle *universal = qobject_cast<QQuickUniversalStyle *>(child);
        if (universal)
            universal->inheritForeground(m_foreground, m_hasForeground);
    }
}

void QQuickUniversalStyle::resetForeground()
{
    if (!m_explicitForeground)
        return;

    m_hasForeground = false;
    m_explicitForeground = false;
    QQuickUniversalStyle *universal = qobject_cast<QQuickUniversalStyle *>(parentStyle());
    inheritForeground(universal ? universal->m_foreground : GlobalForeground, universal ? universal->m_hasForeground : false);
}

QVariant QQuickUniversalStyle::background() const
{
    if (m_hasBackground)
        return QColor::fromRgba(m_background);
    return altHighColor();
}

void QQuickUniversalStyle::setBackground(const QVariant &var)
{
    QRgb background = 0;
    if (!variantToRgba(var, "background", &background))
        return;

    m_hasBackground = true;
    m_explicitBackground = true;
    if (m_background == background)
        return;

    m_background = background;
    propagateBackground();
    emit backgroundChanged();
}

void QQuickUniversalStyle::inheritBackground(QRgb background, bool has)
{
    if (m_explicitBackground || m_background == background)
        return;

    m_hasBackground = has;
    m_background = background;
    propagateBackground();
    emit backgroundChanged();
}

void QQuickUniversalStyle::propagateBackground()
{
    const auto styles = childStyles();
    for (QQuickStyleAttached *child : styles) {
        QQuickUniversalStyle *universal = qobject_cast<QQuickUniversalStyle *>(child);
        if (universal)
            universal->inheritBackground(m_background, m_hasBackground);
    }
}

void QQuickUniversalStyle::resetBackground()
{
    if (!m_explicitBackground)
        return;

    m_hasBackground = false;
    m_explicitBackground = false;
    QQuickUniversalStyle *universal = qobject_cast<QQuickUniversalStyle *>(parentStyle());
    inheritBackground(universal ? universal->m_background : GlobalBackground, universal ? universal->m_hasBackground : false);
}

QColor QQuickUniversalStyle::color(Color color) const
{
    return qquickuniversal_accent_color(color);
}

QColor QQuickUniversalStyle::altHighColor() const
{
    return systemColor(AltHigh);
}

QColor QQuickUniversalStyle::altLowColor() const
{
    return systemColor(AltLow);
}

QColor QQuickUniversalStyle::altMediumColor() const
{
    return systemColor(AltMedium);
}

QColor QQuickUniversalStyle::altMediumHighColor() const
{
    return systemColor(AltMediumHigh);
}

QColor QQuickUniversalStyle::altMediumLowColor() const
{
    return systemColor(AltMediumLow);
}

QColor QQuickUniversalStyle::baseHighColor() const
{
    return systemColor(BaseHigh);
}

QColor QQuickUniversalStyle::baseLowColor() const
{
    return systemColor(BaseLow);
}

QColor QQuickUniversalStyle::baseMediumColor() const
{
    return systemColor(BaseMedium);
}

QColor QQuickUniversalStyle::baseMediumHighColor() const
{
    return systemColor(BaseMediumHigh);
}

QColor QQuickUniversalStyle::baseMediumLowColor() const
{
    return systemColor(BaseMediumLow);
}

QColor QQuickUniversalStyle::chromeAltLowColor() const
{
    return systemColor(ChromeAltLow);
}

QColor QQuickUniversalStyle::chromeBlackHighColor() const
{
    return systemColor(ChromeBlackHigh);
}

QColor QQuickUniversalStyle::chromeBlackLowColor() const
{
    return systemColor(ChromeBlackLow);
}

QColor QQuickUniversalStyle::chromeBlackMediumLowColor() const
{
    return systemColor(ChromeBlackMediumLow);
}

QColor QQuickUniversalStyle::chromeBlackMediumColor() const
{
    return systemColor(ChromeBlackMedium);
}

QColor QQuickUniversalStyle::chromeDisabledHighColor() const
{
    return systemColor(ChromeDisabledHigh);
}

QColor QQuickUniversalStyle::chromeDisabledLowColor() const
{
    return systemColor(ChromeDisabledLow);
}

QColor QQuickUniversalStyle::chromeHighColor() const
{
    return systemColor(ChromeHigh);
}

QColor QQuickUniversalStyle::chromeLowColor() const
{
    return systemColor(ChromeLow);
}

QColor QQuickUniversalStyle::chromeMediumColor() const
{
    return systemColor(ChromeMedium);
}

QColor QQuickUniversalStyle::chromeMediumLowColor() const
{
    return systemColor(ChromeMediumLow);
}

QColor QQuickUniversalStyle::chromeWhiteColor() const
{
    return systemColor(ChromeWhite);
}

QColor QQuickUniversalStyle::listLowColor() const
{
    return systemColor(ListLow);
}

QColor QQuickUniversalStyle::listMediumColor() const
{
    return systemColor(ListMedium);
}

QColor QQuickUniversalStyle::systemColor(SystemColor role) const
{
    return QColor::fromRgba(m_theme == QQuickUniversalStyle::Dark ? qquickuniversal_dark_color(role) : qquickuniversal_light_color(role));
}

void QQuickUniversalStyle::parentStyleChange(QQuickStyleAttached *newParent, QQuickStyleAttached *oldParent)
{
    Q_UNUSED(oldParent);
    QQuickUniversalStyle *universal = qobject_cast<QQuickUniversalStyle *>(newParent);
    if (universal) {
        inheritTheme(universal->theme());
        inheritAccent(universal->m_accent);
        inheritForeground(universal->m_foreground, universal->m_hasForeground);
        inheritBackground(universal->m_background, universal->m_hasBackground);
    }
}

template <typename Enum>
static Enum toEnumValue(const QByteArray &value, bool *ok)
{
    QMetaEnum enumeration = QMetaEnum::fromType<Enum>();
    return static_cast<Enum>(enumeration.keyToValue(value, ok));
}

static QByteArray resolveSetting(const QByteArray &env, const QSharedPointer<QSettings> &settings, const QString &name)
{
    QByteArray value = qgetenv(env);
    if (value.isNull() && !settings.isNull())
        value = settings->value(name).toByteArray();
    return value;
}

void QQuickUniversalStyle::init()
{
    static bool globalsInitialized = false;
    if (!globalsInitialized) {
        QSharedPointer<QSettings> settings = QQuickStyleAttached::settings(QStringLiteral("Universal"));

        bool ok = false;
        QByteArray themeValue = resolveSetting("QT_QUICK_CONTROLS_UNIVERSAL_THEME", settings, QStringLiteral("Theme"));
        Theme themeEnum = toEnumValue<Theme>(themeValue, &ok);
        if (ok)
            GlobalTheme = m_theme = qquickuniversal_effective_theme(themeEnum);
        else if (!themeValue.isEmpty())
            qWarning().nospace().noquote() << "Universal: unknown theme value: " << themeValue;

        QByteArray accentValue = resolveSetting("QT_QUICK_CONTROLS_UNIVERSAL_ACCENT", settings, QStringLiteral("Accent"));
        Color accentEnum = toEnumValue<Color>(accentValue, &ok);
        if (ok) {
            GlobalAccent = m_accent = qquickuniversal_accent_color(accentEnum);
        } else if (!accentValue.isEmpty()) {
            QColor color(accentValue.constData());
            if (color.isValid())
                GlobalAccent = m_accent = color.rgba();
            else
                qWarning().nospace().noquote() << "Universal: unknown accent value: " << accentValue;
        }

        QByteArray foregroundValue = resolveSetting("QT_QUICK_CONTROLS_UNIVERSAL_FOREGROUND", settings, QStringLiteral("Foreground"));
        Color foregroundEnum = toEnumValue<Color>(foregroundValue, &ok);
        if (ok) {
            GlobalForeground = m_foreground = qquickuniversal_accent_color(foregroundEnum);
            HasGlobalForeground = m_hasForeground = true;
        } else if (!foregroundValue.isEmpty()) {
            QColor color(foregroundValue.constData());
            if (color.isValid()) {
                GlobalForeground = m_foreground = color.rgba();
                HasGlobalForeground = m_hasForeground = true;
            } else {
                qWarning().nospace().noquote() << "Universal: unknown foreground value: " << foregroundValue;
            }
        }

        QByteArray backgroundValue = resolveSetting("QT_QUICK_CONTROLS_UNIVERSAL_BACKGROUND", settings, QStringLiteral("Background"));
        Color backgroundEnum = toEnumValue<Color>(backgroundValue, &ok);
        if (ok) {
            GlobalBackground = m_background = qquickuniversal_accent_color(backgroundEnum);
            HasGlobalBackground = m_hasBackground = true;
        } else if (!backgroundValue.isEmpty()) {
            QColor color(backgroundValue.constData());
            if (color.isValid()) {
                GlobalBackground = m_background = color.rgba();
                HasGlobalBackground = m_hasBackground = true;
            } else {
                qWarning().nospace().noquote() << "Universal: unknown background value: " << backgroundValue;
            }
        }

        globalsInitialized = true;
    }

    QQuickStyleAttached::init(); // TODO: lazy init?
}

bool QQuickUniversalStyle::variantToRgba(const QVariant &var, const char *name, QRgb *rgba) const
{
    if (var.type() == QVariant::Int) {
        int val = var.toInt();
        if (val < Lime || val > Taupe) {
            qmlInfo(parent()) << "unknown Universal." << name << " value: " << val;
            return false;
        }
        *rgba = qquickuniversal_accent_color(static_cast<Color>(val));
    } else {
        int val = QMetaEnum::fromType<Color>().keyToValue(var.toByteArray());
        if (val != -1) {
            *rgba = qquickuniversal_accent_color(static_cast<Color>(val));
        } else {
            QColor color(var.toString());
            if (!color.isValid()) {
                qmlInfo(parent()) << "unknown Universal." << name << " value: " << var.toString();
                return false;
            }
            *rgba = color.rgba();
        }
    }
    return true;
}

QT_END_NAMESPACE
