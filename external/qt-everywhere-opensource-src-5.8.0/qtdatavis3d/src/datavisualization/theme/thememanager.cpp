/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "thememanager_p.h"
#include "q3dtheme_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

const float defaultBuiltInColorLevel = 0.7f; // for built-in gradient themes
const float defaultColorLevel = 0.5f; // for built-in uniform themes

ThemeManager::ThemeManager(Abstract3DController *controller)
    : m_activeTheme(0),
      m_controller(controller)
{
}

ThemeManager::~ThemeManager()
{
}

void ThemeManager::addTheme(Q3DTheme *theme)
{
    Q_ASSERT(theme);
    ThemeManager *owner = qobject_cast<ThemeManager *>(theme->parent());
    if (owner != this) {
        Q_ASSERT_X(!owner, "addTheme", "Theme already attached to a graph.");
        theme->setParent(this);
    }
    if (!m_themes.contains(theme))
        m_themes.append(theme);
}

void ThemeManager::releaseTheme(Q3DTheme *theme)
{
    if (theme && m_themes.contains(theme)) {
        // Clear the default status from released default theme
        if (theme->d_ptr->isDefaultTheme())
            theme->d_ptr->setDefaultTheme(false);

        // If the theme is in use, replace it with a temporary one
        if (theme == m_activeTheme)
            setActiveTheme(0);

        m_themes.removeAll(theme);
        theme->setParent(0);
    }
}

void ThemeManager::setActiveTheme(Q3DTheme *theme)
{
    // Setting null theme indicates using default theme
    if (!theme) {
        theme = new Q3DTheme;
        theme->d_ptr->setDefaultTheme(true);
    }

    // If the old theme is default theme, delete it
    Q3DTheme *oldTheme = m_activeTheme;
    if (oldTheme) {
        if (oldTheme->d_ptr->isDefaultTheme()) {
            m_themes.removeAll(oldTheme);
            delete oldTheme;
            oldTheme = 0;
        } else {
            // Disconnect the old theme from use
            disconnect(m_activeTheme->d_ptr.data(), 0, m_controller, 0);
            disconnect(m_activeTheme, 0, m_controller, 0);
        }
    }

    // Assume ownership
    addTheme(theme);

    m_activeTheme = theme;

    // Reset all bits to dirty for sync
    m_activeTheme->d_ptr->resetDirtyBits();

    // Connect signals from new one
    connectThemeSignals();
}

Q3DTheme *ThemeManager::activeTheme() const
{
    return m_activeTheme;
}

QList<Q3DTheme *> ThemeManager::themes() const
{
    return m_themes;
}

void ThemeManager::connectThemeSignals()
{
    connect(m_activeTheme, &Q3DTheme::colorStyleChanged,
            m_controller, &Abstract3DController::handleThemeColorStyleChanged);
    connect(m_activeTheme, &Q3DTheme::baseColorsChanged,
            m_controller, &Abstract3DController::handleThemeBaseColorsChanged);
    connect(m_activeTheme, &Q3DTheme::singleHighlightColorChanged,
            m_controller, &Abstract3DController::handleThemeSingleHighlightColorChanged);
    connect(m_activeTheme, &Q3DTheme::multiHighlightColorChanged,
            m_controller, &Abstract3DController::handleThemeMultiHighlightColorChanged);
    connect(m_activeTheme, &Q3DTheme::baseGradientsChanged,
            m_controller, &Abstract3DController::handleThemeBaseGradientsChanged);
    connect(m_activeTheme, &Q3DTheme::singleHighlightGradientChanged,
            m_controller, &Abstract3DController::handleThemeSingleHighlightGradientChanged);
    connect(m_activeTheme, &Q3DTheme::multiHighlightGradientChanged,
            m_controller, &Abstract3DController::handleThemeMultiHighlightGradientChanged);
    connect(m_activeTheme, &Q3DTheme::typeChanged,
            m_controller, &Abstract3DController::handleThemeTypeChanged);

    connect(m_activeTheme->d_ptr.data(), &Q3DThemePrivate::needRender,
            m_controller, &Abstract3DController::needRender);
}

void ThemeManager::setPredefinedPropertiesToTheme(Q3DTheme *theme, Q3DTheme::Theme type)
{
    QList<QColor> baseColors;
    QList<QLinearGradient> baseGradients;
    switch (type) {
    case Q3DTheme::ThemeQt: {
        baseColors.append(QColor(QRgb(0x80c342)));
        baseColors.append(QColor(QRgb(0x469835)));
        baseColors.append(QColor(QRgb(0x006325)));
        baseColors.append(QColor(QRgb(0x5caa15)));
        baseColors.append(QColor(QRgb(0x328930)));

        baseGradients.append(createGradient(baseColors.at(0), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(1), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(2), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(3), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(4), defaultColorLevel));

        setBackgroundEnabled(theme, true);
        setGridEnabled(theme, true);
        setFont(theme, QFont(QStringLiteral("Arial")));
        setLabelBackgroundEnabled(theme, true);
        setLightColor(theme, Qt::white);
        setBaseColors(theme, baseColors);
        setBackgroundColor(theme, QColor(QRgb(0xffffff)));
        setWindowColor(theme, QColor(QRgb(0xffffff)));
        setTextColor(theme, QColor(QRgb(0x35322f)));
        setTextBackgroundColor(theme, QColor(0xff, 0xff, 0xff, 0x99));
        setGridLineColor(theme, QColor(QRgb(0xd7d6d5)));
        setSingleHighlightColor(theme, QColor(QRgb(0x14aaff)));
        setMultiHighlightColor(theme, QColor(QRgb(0x6400aa)));
        setLightStrength(theme, 5.0f);
        setAmbientLightStrength(theme, 0.5f);
        setHighlightLightStrength(theme, 5.0f);
        setLabelBorderEnabled(theme, true);
        setColorStyle(theme, Q3DTheme::ColorStyleUniform);
        setBaseGradients(theme, baseGradients);
        setSingleHighlightGradient(theme, createGradient(QColor(QRgb(0x14aaff)),
                                                         defaultColorLevel));
        setMultiHighlightGradient(theme, createGradient(QColor(QRgb(0x6400aa)),
                                                        defaultColorLevel));
        break;
    }

    case Q3DTheme::ThemePrimaryColors: {
        baseColors.append(QColor(QRgb(0xffe400)));
        baseColors.append(QColor(QRgb(0xfaa106)));
        baseColors.append(QColor(QRgb(0xf45f0d)));
        baseColors.append(QColor(QRgb(0xfcba04)));
        baseColors.append(QColor(QRgb(0xf7800a)));

        baseGradients.append(createGradient(baseColors.at(0), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(1), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(2), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(3), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(4), defaultColorLevel));

        setBackgroundEnabled(theme, true);
        setGridEnabled(theme, true);
        setFont(theme, QFont(QStringLiteral("Arial")));
        setLabelBackgroundEnabled(theme, true);
        setLightColor(theme, Qt::white);
        setBaseColors(theme, baseColors);
        setBackgroundColor(theme, QColor(QRgb(0xffffff)));
        setWindowColor(theme, QColor(QRgb(0xffffff)));
        setTextColor(theme, QColor(QRgb(0x000000)));
        setTextBackgroundColor(theme, QColor(0xff, 0xff, 0xff, 0x99));
        setGridLineColor(theme, QColor(QRgb(0xd7d6d5)));
        setSingleHighlightColor(theme, QColor(QRgb(0x27beee)));
        setMultiHighlightColor(theme, QColor(QRgb(0xee1414)));
        setLightStrength(theme, 5.0f);
        setAmbientLightStrength(theme, 0.5f);
        setHighlightLightStrength(theme, 5.0f);
        setLabelBorderEnabled(theme, false);
        setColorStyle(theme, Q3DTheme::ColorStyleUniform);
        setBaseGradients(theme, baseGradients);
        setSingleHighlightGradient(theme, createGradient(QColor(QRgb(0x27beee)),
                                                         defaultColorLevel));
        setMultiHighlightGradient(theme, createGradient(QColor(QRgb(0xee1414)),
                                                        defaultColorLevel));
        break;
    }

    case Q3DTheme::ThemeDigia: {
        baseColors.append(QColor(QRgb(0xeaeaea)));
        baseColors.append(QColor(QRgb(0xa0a0a0)));
        baseColors.append(QColor(QRgb(0x626262)));
        baseColors.append(QColor(QRgb(0xbebebe)));
        baseColors.append(QColor(QRgb(0x818181)));

        baseGradients.append(createGradient(baseColors.at(0), defaultBuiltInColorLevel));
        baseGradients.append(createGradient(baseColors.at(1), defaultBuiltInColorLevel));
        baseGradients.append(createGradient(baseColors.at(2), defaultBuiltInColorLevel));
        baseGradients.append(createGradient(baseColors.at(3), defaultBuiltInColorLevel));
        baseGradients.append(createGradient(baseColors.at(4), defaultBuiltInColorLevel));

        setBackgroundEnabled(theme, true);
        setGridEnabled(theme, true);
        setFont(theme, QFont(QStringLiteral("Arial")));
        setLabelBackgroundEnabled(theme, true);
        setLightColor(theme, Qt::white);
        setBaseColors(theme, baseColors);
        setBackgroundColor(theme, QColor(QRgb(0xffffff)));
        setWindowColor(theme, QColor(QRgb(0xffffff)));
        setTextColor(theme, QColor(QRgb(0x000000)));
        setTextBackgroundColor(theme, QColor(0xff, 0xff, 0xff, 0x80));
        setGridLineColor(theme, QColor(QRgb(0xd7d6d5)));
        setSingleHighlightColor(theme, QColor(QRgb(0xfa0000)));
        setMultiHighlightColor(theme, QColor(QRgb(0x333333)));
        setLightStrength(theme, 5.0f);
        setAmbientLightStrength(theme, 0.5f);
        setHighlightLightStrength(theme, 5.0f);
        setLabelBorderEnabled(theme, false);
        setColorStyle(theme, Q3DTheme::ColorStyleObjectGradient);
        setBaseGradients(theme, baseGradients);
        setSingleHighlightGradient(theme, createGradient(QColor(QRgb(0xfa0000)),
                                                         defaultBuiltInColorLevel));
        setMultiHighlightGradient(theme, createGradient(QColor(QRgb(0x333333)),
                                                        defaultBuiltInColorLevel));
        break;
    }

    case Q3DTheme::ThemeStoneMoss: {
        baseColors.append(QColor(QRgb(0xbeb32b)));
        baseColors.append(QColor(QRgb(0x928327)));
        baseColors.append(QColor(QRgb(0x665423)));
        baseColors.append(QColor(QRgb(0xa69929)));
        baseColors.append(QColor(QRgb(0x7c6c25)));

        baseGradients.append(createGradient(baseColors.at(0), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(1), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(2), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(3), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(4), defaultColorLevel));

        setBackgroundEnabled(theme, true);
        setGridEnabled(theme, true);
        setFont(theme, QFont(QStringLiteral("Arial")));
        setLabelBackgroundEnabled(theme, true);
        setLightColor(theme, Qt::white);
        setBaseColors(theme, baseColors);
        setBackgroundColor(theme, QColor(QRgb(0x4d4d4f)));
        setWindowColor(theme, QColor(QRgb(0x4d4d4f)));
        setTextColor(theme, QColor(QRgb(0xffffff)));
        setTextBackgroundColor(theme, QColor(0x4d, 0x4d, 0x4f, 0xcd));
        setGridLineColor(theme, QColor(QRgb(0x3e3e40)));
        setSingleHighlightColor(theme, QColor(QRgb(0xfbf6d6)));
        setMultiHighlightColor(theme, QColor(QRgb(0x442f20)));
        setLightStrength(theme, 5.0f);
        setAmbientLightStrength(theme, 0.5f);
        setHighlightLightStrength(theme, 5.0f);
        setLabelBorderEnabled(theme, true);
        setColorStyle(theme, Q3DTheme::ColorStyleUniform);
        setBaseGradients(theme, baseGradients);
        setSingleHighlightGradient(theme, createGradient(QColor(QRgb(0xfbf6d6)),
                                                         defaultColorLevel));
        setMultiHighlightGradient(theme, createGradient(QColor(QRgb(0x442f20)),
                                                        defaultColorLevel));
        break;
    }

    case Q3DTheme::ThemeArmyBlue: {
        baseColors.append(QColor(QRgb(0x495f76)));
        baseColors.append(QColor(QRgb(0x81909f)));
        baseColors.append(QColor(QRgb(0xbec5cd)));
        baseColors.append(QColor(QRgb(0x687a8d)));
        baseColors.append(QColor(QRgb(0xa3aeb9)));

        baseGradients.append(createGradient(baseColors.at(0), defaultBuiltInColorLevel));
        baseGradients.append(createGradient(baseColors.at(1), defaultBuiltInColorLevel));
        baseGradients.append(createGradient(baseColors.at(2), defaultBuiltInColorLevel));
        baseGradients.append(createGradient(baseColors.at(3), defaultBuiltInColorLevel));
        baseGradients.append(createGradient(baseColors.at(4), defaultBuiltInColorLevel));

        setBackgroundEnabled(theme, true);
        setGridEnabled(theme, true);
        setFont(theme, QFont(QStringLiteral("Arial")));
        setLabelBackgroundEnabled(theme, true);
        setLightColor(theme, Qt::white);
        setBaseColors(theme, baseColors);
        setBackgroundColor(theme, QColor(QRgb(0xd5d6d7)));
        setWindowColor(theme, QColor(QRgb(0xd5d6d7)));
        setTextColor(theme, QColor(QRgb(0x000000)));
        setTextBackgroundColor(theme, QColor(0xd5, 0xd6, 0xd7, 0xcd));
        setGridLineColor(theme, QColor(QRgb(0xaeadac)));
        setSingleHighlightColor(theme, QColor(QRgb(0x2aa2f9)));
        setMultiHighlightColor(theme, QColor(QRgb(0x103753)));
        setLightStrength(theme, 5.0f);
        setAmbientLightStrength(theme, 0.5f);
        setHighlightLightStrength(theme, 5.0f);
        setLabelBorderEnabled(theme, false);
        setColorStyle(theme, Q3DTheme::ColorStyleObjectGradient);
        setBaseGradients(theme, baseGradients);
        setSingleHighlightGradient(theme, createGradient(QColor(QRgb(0x2aa2f9)),
                                                         defaultBuiltInColorLevel));
        setMultiHighlightGradient(theme, createGradient(QColor(QRgb(0x103753)),
                                                        defaultBuiltInColorLevel));
        break;
    }

    case Q3DTheme::ThemeRetro: {
        baseColors.append(QColor(QRgb(0x533b23)));
        baseColors.append(QColor(QRgb(0x83715a)));
        baseColors.append(QColor(QRgb(0xb3a690)));
        baseColors.append(QColor(QRgb(0x6b563e)));
        baseColors.append(QColor(QRgb(0x9b8b75)));

        baseGradients.append(createGradient(baseColors.at(0), defaultBuiltInColorLevel));
        baseGradients.append(createGradient(baseColors.at(1), defaultBuiltInColorLevel));
        baseGradients.append(createGradient(baseColors.at(2), defaultBuiltInColorLevel));
        baseGradients.append(createGradient(baseColors.at(3), defaultBuiltInColorLevel));
        baseGradients.append(createGradient(baseColors.at(4), defaultBuiltInColorLevel));

        setBackgroundEnabled(theme, true);
        setGridEnabled(theme, true);
        setFont(theme, QFont(QStringLiteral("Arial")));
        setLabelBackgroundEnabled(theme, true);
        setLightColor(theme, Qt::white);
        setBaseColors(theme, baseColors);
        setBackgroundColor(theme, QColor(QRgb(0xe9e2ce)));
        setWindowColor(theme, QColor(QRgb(0xe9e2ce)));
        setTextColor(theme, QColor(QRgb(0x000000)));
        setTextBackgroundColor(theme, QColor(0xe9, 0xe2, 0xce, 0xc0));
        setGridLineColor(theme, QColor(QRgb(0xd0c0b0)));
        setSingleHighlightColor(theme, QColor(QRgb(0x8ea317)));
        setMultiHighlightColor(theme, QColor(QRgb(0xc25708)));
        setLightStrength(theme, 5.0f);
        setAmbientLightStrength(theme, 0.5f);
        setHighlightLightStrength(theme, 5.0f);
        setLabelBorderEnabled(theme, false);
        setColorStyle(theme, Q3DTheme::ColorStyleObjectGradient);
        setBaseGradients(theme, baseGradients);
        setSingleHighlightGradient(theme, createGradient(QColor(QRgb(0x8ea317)),
                                                         defaultBuiltInColorLevel));
        setMultiHighlightGradient(theme, createGradient(QColor(QRgb(0xc25708)),
                                                        defaultBuiltInColorLevel));
        break;
    }

    case Q3DTheme::ThemeEbony: {
        baseColors.append(QColor(QRgb(0xffffff)));
        baseColors.append(QColor(QRgb(0x999999)));
        baseColors.append(QColor(QRgb(0x474747)));
        baseColors.append(QColor(QRgb(0xc7c7c7)));
        baseColors.append(QColor(QRgb(0x6b6b6b)));

        baseGradients.append(createGradient(baseColors.at(0), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(1), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(2), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(3), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(4), defaultColorLevel));

        setBackgroundEnabled(theme, true);
        setGridEnabled(theme, true);
        setFont(theme, QFont(QStringLiteral("Arial")));
        setLabelBackgroundEnabled(theme, true);
        setLightColor(theme, Qt::white);
        setBaseColors(theme, baseColors);
        setBackgroundColor(theme, QColor(QRgb(0x000000)));
        setWindowColor(theme, QColor(QRgb(0x000000)));
        setTextColor(theme, QColor(QRgb(0xaeadac)));
        setTextBackgroundColor(theme, QColor(0x00, 0x00, 0x00, 0xcd));
        setGridLineColor(theme, QColor(QRgb(0x35322f)));
        setSingleHighlightColor(theme, QColor(QRgb(0xf5dc0d)));
        setMultiHighlightColor(theme, QColor(QRgb(0xd72222)));
        setLightStrength(theme, 5.0f);
        setAmbientLightStrength(theme, 0.5f);
        setHighlightLightStrength(theme, 5.0f);
        setLabelBorderEnabled(theme, false);
        setColorStyle(theme, Q3DTheme::ColorStyleUniform);
        setBaseGradients(theme, baseGradients);
        setSingleHighlightGradient(theme, createGradient(QColor(QRgb(0xf5dc0d)),
                                                         defaultColorLevel));
        setMultiHighlightGradient(theme, createGradient(QColor(QRgb(0xd72222)),
                                                        defaultColorLevel));
        break;
    }

    case Q3DTheme::ThemeIsabelle: {
        baseColors.append(QColor(QRgb(0xf9d900)));
        baseColors.append(QColor(QRgb(0xf09603)));
        baseColors.append(QColor(QRgb(0xe85506)));
        baseColors.append(QColor(QRgb(0xf5b802)));
        baseColors.append(QColor(QRgb(0xec7605)));

        baseGradients.append(createGradient(baseColors.at(0), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(1), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(2), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(3), defaultColorLevel));
        baseGradients.append(createGradient(baseColors.at(4), defaultColorLevel));

        setBackgroundEnabled(theme, true);
        setGridEnabled(theme, true);
        setFont(theme, QFont(QStringLiteral("Arial")));
        setLabelBackgroundEnabled(theme, true);
        setLightColor(theme, Qt::white);
        setBaseColors(theme, baseColors);
        setBackgroundColor(theme, QColor(QRgb(0x000000)));
        setWindowColor(theme, QColor(QRgb(0x000000)));
        setTextColor(theme, QColor(QRgb(0xaeadac)));
        setTextBackgroundColor(theme, QColor(0x00, 0x00, 0x00, 0xc0));
        setGridLineColor(theme, QColor(QRgb(0x35322f)));
        setSingleHighlightColor(theme, QColor(QRgb(0xfff7cc)));
        setMultiHighlightColor(theme, QColor(QRgb(0xde0a0a)));
        setLightStrength(theme, 5.0f);
        setAmbientLightStrength(theme, 0.5f);
        setHighlightLightStrength(theme, 5.0f);
        setLabelBorderEnabled(theme, false);
        setColorStyle(theme, Q3DTheme::ColorStyleUniform);
        setBaseGradients(theme, baseGradients);
        setSingleHighlightGradient(theme, createGradient(QColor(QRgb(0xfff7cc)),
                                                         defaultColorLevel));
        setMultiHighlightGradient(theme, createGradient(QColor(QRgb(0xde0a0a)),
                                                        defaultColorLevel));
        break;
    }
    default:
        break;
    }
}

QLinearGradient ThemeManager::createGradient(const QColor &color, float colorLevel)
{
    QColor startColor;
    QLinearGradient gradient = QLinearGradient(qreal(gradientTextureWidth),
                                               qreal(gradientTextureHeight),
                                               0.0, 0.0);;
    startColor.setRed(color.red() * colorLevel);
    startColor.setGreen(color.green() * colorLevel);
    startColor.setBlue(color.blue() * colorLevel);
    gradient.setColorAt(0.0, startColor);
    gradient.setColorAt(1.0, color);
    return gradient;
}

void ThemeManager::setBaseColors(Q3DTheme *theme, const QList<QColor> &colors)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.baseColorDirty)
        theme->setBaseColors(colors);
}

void ThemeManager::setBackgroundColor(Q3DTheme *theme, const QColor &color)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.backgroundColorDirty)
        theme->setBackgroundColor(color);
}

void ThemeManager::setWindowColor(Q3DTheme *theme, const QColor &color)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.windowColorDirty)
        theme->setWindowColor(color);
}

void ThemeManager::setTextColor(Q3DTheme *theme, const QColor &color)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.labelTextColorDirty)
        theme->setLabelTextColor(color);
}

void ThemeManager::setTextBackgroundColor(Q3DTheme *theme, const QColor &color)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.labelBackgroundColorDirty)
        theme->setLabelBackgroundColor(color);
}

void ThemeManager::setGridLineColor(Q3DTheme *theme, const QColor &color)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.gridLineColorDirty)
        theme->setGridLineColor(color);
}

void ThemeManager::setSingleHighlightColor(Q3DTheme *theme, const QColor &color)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.singleHighlightColorDirty)
        theme->setSingleHighlightColor(color);
}

void ThemeManager::setMultiHighlightColor(Q3DTheme *theme, const QColor &color)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.multiHighlightColorDirty)
        theme->setMultiHighlightColor(color);
}

void ThemeManager::setLightColor(Q3DTheme *theme, const QColor &color)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.lightColorDirty)
        theme->setLightColor(color);
}

void ThemeManager::setBaseGradients(Q3DTheme *theme, const QList<QLinearGradient> &gradients)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.baseGradientDirty)
        theme->setBaseGradients(gradients);
}

void ThemeManager::setSingleHighlightGradient(Q3DTheme *theme, const QLinearGradient &gradient)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.singleHighlightGradientDirty)
        theme->setSingleHighlightGradient(gradient);
}

void ThemeManager::setMultiHighlightGradient(Q3DTheme *theme, const QLinearGradient &gradient)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.multiHighlightGradientDirty)
        theme->setMultiHighlightGradient(gradient);
}

void ThemeManager::setLightStrength(Q3DTheme *theme, float strength)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.lightStrengthDirty)
        theme->setLightStrength(strength);
}

void ThemeManager::setAmbientLightStrength(Q3DTheme *theme, float strength)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.ambientLightStrengthDirty)
        theme->setAmbientLightStrength(strength);
}

void ThemeManager::setHighlightLightStrength(Q3DTheme *theme, float strength)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.highlightLightStrengthDirty)
        theme->setHighlightLightStrength(strength);
}

void ThemeManager::setLabelBorderEnabled(Q3DTheme *theme, bool enabled)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.labelBorderEnabledDirty)
        theme->setLabelBorderEnabled(enabled);
}

void ThemeManager::setFont(Q3DTheme *theme, const QFont &font)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.fontDirty)
        theme->setFont(font);
}

void ThemeManager::setBackgroundEnabled(Q3DTheme *theme, bool enabled)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.backgroundEnabledDirty)
        theme->setBackgroundEnabled(enabled);
}

void ThemeManager::setGridEnabled(Q3DTheme *theme, bool enabled)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.gridEnabledDirty)
        theme->setGridEnabled(enabled);
}

void ThemeManager::setLabelBackgroundEnabled(Q3DTheme *theme, bool enabled)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.labelBackgroundEnabledDirty)
        theme->setLabelBackgroundEnabled(enabled);
}

void ThemeManager::setColorStyle(Q3DTheme *theme, Q3DTheme::ColorStyle style)
{
    if (theme->d_ptr->isForcePredefinedType() || !theme->d_ptr->m_dirtyBits.colorStyleDirty)
        theme->setColorStyle(style);
}

QT_END_NAMESPACE_DATAVISUALIZATION
