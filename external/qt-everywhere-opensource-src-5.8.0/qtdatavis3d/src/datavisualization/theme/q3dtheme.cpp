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

#include "q3dtheme_p.h"
#include "thememanager_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class Q3DTheme
 * \inmodule QtDataVisualization
 * \brief Q3DTheme class provides a visual style for graphs.
 * \since QtDataVisualization 1.0
 *
 * Q3DTheme is used to specify visual properties that affect the whole graph. There are several
 * built-in themes that can be used directly, or be modified freely. User can also create a theme
 * from scratch using \c ThemeUserDefined. Creating a theme using the default constructor
 * produces a new user-defined theme.
 *
 * \section1 Properties controlled by theme
 * \table
 *   \header
 *     \li Property
 *     \li Effect
 *     \li Default in ThemeUserDefined
 *   \row
 *     \li ambientLightStrength
 *     \li The strength of the ambient light in the graph. This affects how evenly and brightly the
 *         colors are shown throughout the graph regardless of light position.
 *     \li 0.25
 *   \row
 *     \li backgroundColor
 *     \li Color of the graph background.
 *     \li Qt::black
 *   \row
 *     \li backgroundEnabled
 *     \li Is the graph background drawn or not.
 *     \li true
 *   \row
 *     \li baseColors
 *     \li List of colors for the objects in the graph. Colors are applied to series one by one.
 *         If there are more series than colors, the color list is reused from the start.
 *         The colors in this property are used if colorStyle is ColorStyleUniform. This can be
 *         overridden by setting the \l{QAbstract3DSeries::baseColor}{baseColor} explicitly in series.
 *     \li Qt::black
 *   \row
 *     \li baseGradients
 *     \li List of gradients for the objects in the graph. Gradients are applied to series one by
 *         one. If there are more series than gradients, the gradient list is reused from the start.
 *         The gradients in this property are used if colorStyle is ColorStyleObjectGradient or
 *         ColorStyleRangeGradient. This can be overridden by setting the
 *         \l{QAbstract3DSeries::baseGradient}{baseGradient} explicitly in series.
 *     \li QLinearGradient(). Essentially fully black.
 *   \row
 *     \li colorStyle
 *     \li The color style of the objects in the graph. See ColorStyle for detailed description of
 *         the styles. This can be overridden by setting the
 *         \l{QAbstract3DSeries::colorStyle}{colorStyle} explicitly in series.
 *     \li ColorStyleUniform
 *   \row
 *     \li \l font
 *     \li The font to be used for labels.
 *     \li QFont()
 *   \row
 *     \li gridEnabled
 *     \li Is the grid on the background drawn or not. This affects all grid lines.
 *     \li true
 *   \row
 *     \li gridLineColor
 *     \li The color for the grid lines.
 *     \li Qt::white
 *   \row
 *     \li highlightLightStrength
 *     \li The specular light strength for highlighted objects.
 *     \li 7.5
 *   \row
 *     \li labelBackgroundColor
 *     \li The color of the label background. This property has no effect if labelBackgroundEnabled
 *         is \c false.
 *     \li Qt::gray
 *   \row
 *     \li labelBackgroundEnabled
 *     \li Are the labels to be drawn with a background using labelBackgroundColor (including alpha),
 *         or with a fully transparent background. Labels with background are drawn to equal sizes
 *         per axis based on the longest label, and the text is centered in it. Labels without
 *         background are drawn as is and are left/right aligned based on their position in the
 *         graph.
 *     \li true
 *   \row
 *     \li labelBorderEnabled
 *     \li Are the labels to be drawn with a border or not. This property affects only labels with
 *         a background.
 *     \li true
 *   \row
 *     \li labelTextColor
 *     \li The color for the font used in labels.
 *     \li Qt::white
 *   \row
 *     \li lightColor
 *     \li The color of the light. Affects both ambient and specular light.
 *     \li Qt::white
 *   \row
 *     \li lightStrength
 *     \li The strength of the specular light in the graph. This affects the light specified in
 *         Q3DScene.
 *     \li 5.0
 *   \row
 *     \li multiHighlightColor
 *     \li The color to be used for highlighted objects, if \l{Q3DBars::selectionMode}{selectionMode}
 *         of the graph has \c QAbstract3DGraph::SelectionRow or \c QAbstract3DGraph::SelectionColumn flag set.
 *     \li Qt::blue
 *   \row
 *     \li multiHighlightGradient
 *     \li The gradient to be used for highlighted objects, if \l{Q3DBars::selectionMode}{selectionMode}
 *         of the graph has \c QAbstract3DGraph::SelectionRow or \c QAbstract3DGraph::SelectionColumn flag set.
 *     \li QLinearGradient(). Essentially fully black.
 *   \row
 *     \li singleHighlightColor
 *     \li The color to be used for a highlighted object, if \l{Q3DBars::selectionMode}{selectionMode}
 *         of the graph has \c QAbstract3DGraph::SelectionItem flag set.
 *     \li Qt::red
 *   \row
 *     \li singleHighlightGradient
 *     \li The gradient to be used for a highlighted object, if \l{Q3DBars::selectionMode}{selectionMode}
 *         of the graph has \c QAbstract3DGraph::SelectionItem flag set.
 *     \li QLinearGradient(). Essentially fully black.
 *   \row
 *     \li windowColor
 *     \li The color of the window the graph is drawn into.
 *     \li Qt::black
 * \endtable
 *
 * \section1 Usage examples
 *
 * Creating a built-in theme without any modifications:
 *
 * \snippet doc_src_q3dtheme.cpp 0
 *
 * Creating a built-in theme and modifying some properties:
 *
 * \snippet doc_src_q3dtheme.cpp 1
 *
 * Creating a user-defined theme:
 *
 * \snippet doc_src_q3dtheme.cpp 2
 *
 * Creating a built-in theme and modifying some properties after it has been set:
 *
 * \snippet doc_src_q3dtheme.cpp 3
 *
 */

/*!
 * \enum Q3DTheme::ColorStyle
 *
 * Color styles.
 *
 * \value ColorStyleUniform
 *        Objects are rendered in a single color. The color used is specified in baseColors,
 *        singleHighlightColor and multiHighlightColor properties.
 * \value ColorStyleObjectGradient
 *        Objects are colored using a full gradient for each object regardless of object height. The
 *        gradient used is specified in baseGradients, singleHighlightGradient and
 *        multiHighlightGradient properties.
 * \value ColorStyleRangeGradient
 *        Objects are colored using a portion of the full gradient determined by the object's
 *        height and its position on the Y-axis. The gradient used is specified in baseGradients,
 *        singleHighlightGradient and multiHighlightGradient properties.
 */

/*!
 * \enum Q3DTheme::Theme
 *
 * Built-in themes.
 *
 * \value ThemeQt
 *        A light theme with green as the base color.
 * \value ThemePrimaryColors
 *        A light theme with yellow as the base color.
 * \value ThemeDigia
 *        A light theme with gray as the base color.
 * \value ThemeStoneMoss
 *        A medium dark theme with yellow as the base color.
 * \value ThemeArmyBlue
 *        A medium light theme with blue as the base color.
 * \value ThemeRetro
 *        A medium light theme with brown as the base color.
 * \value ThemeEbony
 *        A dark theme with white as the base color.
 * \value ThemeIsabelle
 *        A dark theme with yellow as the base color.
 * \value ThemeUserDefined
 *        A user-defined theme. See \l {Properties controlled by theme} for theme defaults.
 */

/*!
 * \qmltype Theme3D
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.0
 * \ingroup datavisualization_qml
 * \instantiates Q3DTheme
 * \brief A visual style for graphs.
 *
 * This type is used to specify visual properties that affect the whole graph. There are several
 * built-in themes that can be used directly, or be modified freely. User can also create a theme
 * from scratch using \c Theme3D.ThemeUserDefined.
 *
 * For a more complete description, see Q3DTheme.
 *
 * \section1 Usage examples
 *
 * Using a built-in theme without any modifications:
 *
 * \snippet doc_src_q3dtheme.cpp 4
 *
 * Using a built-in theme and modifying some properties:
 *
 * \snippet doc_src_q3dtheme.cpp 5
 *
 * Using a user-defined theme:
 *
 * \snippet doc_src_q3dtheme.cpp 6
 *
 * For Theme3D enums, see \l Q3DTheme::ColorStyle and \l{Q3DTheme::Theme}.
 */

/*!
 * \qmlproperty list<ThemeColor> Theme3D::baseColors
 *
 * List of base colors to be used for all the objects in the graph, series by series. If there
 * are more series than colors, color list wraps and starts again with the first color in the list.
 * Has no immediate effect if colorStyle is not \c Theme3D.ColorStyleUniform.
 */

/*!
 * \qmlproperty color Theme3D::backgroundColor
 *
 * Color for the graph background.
 */

/*!
 * \qmlproperty color Theme3D::windowColor
 *
 * Color for the application window.
 */

/*!
 * \qmlproperty color Theme3D::labelTextColor
 *
 * Color for the font used for labels.
 */

/*!
 * \qmlproperty color Theme3D::labelBackgroundColor
 *
 * Color for the label backgrounds. Has no effect if labelBackgroundEnabled is \c false.
 */

/*!
 * \qmlproperty color Theme3D::gridLineColor
 *
 * Color for the grid lines.
 */

/*!
 * \qmlproperty color Theme3D::singleHighlightColor
 *
 * Highlight color for a highlighted object. Used if \l{AbstractGraph3D::selectionMode}{selectionMode}
 * has \c AbstractGraph3D.SelectionItem flag set.
 */

/*!
 * \qmlproperty color Theme3D::multiHighlightColor
 *
 * Highlight color for highlighted objects. Used if \l{AbstractGraph3D::selectionMode}{selectionMode}
 * has \c AbstractGraph3D.SelectionRow or \c AbstractGraph3D.SelectionColumn flag set.
 */

/*!
 * \qmlproperty color Theme3D::lightColor
 *
 * Color for the specular light defined in Scene3D.
 */

/*!
 * \qmlproperty list<ColorGradient> Theme3D::baseGradients
 *
 * List of base gradients to be used for all the objects in the graph, series by series. If there
 * are more series than gradients, gradient list wraps and starts again with the first gradient in
 * the list
 * Has no immediate effect if colorStyle is \c Theme3D.ColorStyleUniform.
 */

/*!
 * \qmlproperty ColorGradient Theme3D::singleHighlightGradient
 *
 * Highlight gradient for a highlighted object. Used if \l{AbstractGraph3D::selectionMode}{selectionMode}
 * has \c AbstractGraph3D.SelectionItem flag set.
 */

/*!
 * \qmlproperty ColorGradient Theme3D::multiHighlightGradient
 *
 * Highlight gradient for highlighted objects. Used if \l{AbstractGraph3D::selectionMode}{selectionMode}
 * has \c AbstractGraph3D.SelectionRow or \c AbstractGraph3D.SelectionColumn flag set.
 */

/*!
 * \qmlproperty real Theme3D::lightStrength
 *
 * Specular light strength for the whole graph. Value must be between 0.0 and 10.0.
 */

/*!
 * \qmlproperty real Theme3D::ambientLightStrength
 *
 * Ambient light strength for the whole graph. Value must be between 0.0 and 1.0.
 */

/*!
 * \qmlproperty real Theme3D::highlightLightStrength
 *
 * Specular light strength for highlighted objects. Value must be between 0.0 and 10.0.
 */

/*!
 * \qmlproperty bool Theme3D::labelBorderEnabled
 *
 * Set label borders enabled or disabled. Has no effect if labelBackgroundEnabled is \c false.
 */

/*!
 * \qmlproperty font Theme3D::font
 *
 * Set font to be used for labels.
 */

/*!
 * \qmlproperty bool Theme3D::backgroundEnabled
 *
 * Set background enabled or disabled.
 */

/*!
 * \qmlproperty bool Theme3D::gridEnabled
 *
 * Set grid lines enabled or disabled.
 */

/*!
 * \qmlproperty bool Theme3D::labelBackgroundEnabled
 *
 * Set label backgrounds enabled or disabled.
 */

/*!
 * \qmlproperty Theme3D.ColorStyle Theme3D::colorStyle
 *
 * The \a style of the graph colors. One of \c Theme3D.ColorStyle.
 */

/*!
 * \qmlproperty Theme3D.Theme Theme3D::type
 *
 * The type of the theme. If no type is set, the type is \c Theme3D.ThemeUserDefined.
 * Changing the theme type after the item has been constructed will change all other properties
 * of the theme to what the predefined theme specifies. Changing the theme type of the active theme
 * of the graph will also reset all attached series to use the new theme.
 */

/*!
 * Constructs a new theme of type ThemeUserDefined. An optional \a parent parameter
 * can be given and is then passed to QObject constructor.
 */
Q3DTheme::Q3DTheme(QObject *parent)
    : QObject(parent),
      d_ptr(new Q3DThemePrivate(this))
{
}

/*!
 * Constructs a new theme with \a themeType, which can be one of the built-in themes from
 * \l Theme. An optional \a parent parameter can be given and is then passed to QObject
 * constructor.
 */
Q3DTheme::Q3DTheme(Theme themeType, QObject *parent)
    : QObject(parent),
      d_ptr(new Q3DThemePrivate(this))
{
    setType(themeType);
}

/*!
 * \internal
 */
Q3DTheme::Q3DTheme(Q3DThemePrivate *d, Theme themeType,
                   QObject *parent) :
    QObject(parent),
    d_ptr(d)
{
    setType(themeType);
}

/*!
 * Destroys the theme.
 */
Q3DTheme::~Q3DTheme()
{
}

/*!
 * \property Q3DTheme::baseColors
 *
 * List of base colors to be used for all the objects in the graph, series by series. If there
 * are more series than colors, color list wraps and starts again with the first color in the list.
 * Has no immediate effect if colorStyle is not ColorStyleUniform.
 */
void Q3DTheme::setBaseColors(const QList<QColor> &colors)
{
    if (colors.size()) {
        d_ptr->m_dirtyBits.baseColorDirty = true;
        if (d_ptr->m_baseColors != colors) {
            d_ptr->m_baseColors.clear();
            d_ptr->m_baseColors = colors;
            emit baseColorsChanged(colors);
        }
    } else {
        d_ptr->m_baseColors.clear();
    }
}

QList<QColor> Q3DTheme::baseColors() const
{
    return d_ptr->m_baseColors;
}

/*!
 * \property Q3DTheme::backgroundColor
 *
 * Color for the graph background.
 */
void Q3DTheme::setBackgroundColor(const QColor &color)
{
    d_ptr->m_dirtyBits.backgroundColorDirty = true;
    if (d_ptr->m_backgroundColor != color) {
        d_ptr->m_backgroundColor = color;
        emit backgroundColorChanged(color);
        emit d_ptr->needRender();
    }
}

QColor Q3DTheme::backgroundColor() const
{
    return d_ptr->m_backgroundColor;
}

/*!
 * \property Q3DTheme::windowColor
 *
 * Color for the application window.
 */
void Q3DTheme::setWindowColor(const QColor &color)
{
    d_ptr->m_dirtyBits.windowColorDirty = true;
    if (d_ptr->m_windowColor != color) {
        d_ptr->m_windowColor = color;
        emit windowColorChanged(color);
        emit d_ptr->needRender();
    }
}

QColor Q3DTheme::windowColor() const
{
    return d_ptr->m_windowColor;
}

/*!
 * \property Q3DTheme::labelTextColor
 *
 * Color for the font used for labels.
 */
void Q3DTheme::setLabelTextColor(const QColor &color)
{
    d_ptr->m_dirtyBits.labelTextColorDirty = true;
    if (d_ptr->m_textColor != color) {
        d_ptr->m_textColor = color;
        emit labelTextColorChanged(color);
        emit d_ptr->needRender();
    }
}

QColor Q3DTheme::labelTextColor() const
{
    return d_ptr->m_textColor;
}

/*!
 * \property Q3DTheme::labelBackgroundColor
 *
 * Color for the label backgrounds. Has no effect if labelBackgroundEnabled is \c false.
 */
void Q3DTheme::setLabelBackgroundColor(const QColor &color)
{
    d_ptr->m_dirtyBits.labelBackgroundColorDirty = true;
    if (d_ptr->m_textBackgroundColor != color) {
        d_ptr->m_textBackgroundColor = color;
        emit labelBackgroundColorChanged(color);
        emit d_ptr->needRender();
    }
}

QColor Q3DTheme::labelBackgroundColor() const
{
    return d_ptr->m_textBackgroundColor;
}

/*!
 * \property Q3DTheme::gridLineColor
 *
 * Color for the grid lines.
 */
void Q3DTheme::setGridLineColor(const QColor &color)
{
    d_ptr->m_dirtyBits.gridLineColorDirty = true;
    if (d_ptr->m_gridLineColor != color) {
        d_ptr->m_gridLineColor = color;
        emit gridLineColorChanged(color);
        emit d_ptr->needRender();
    }
}

QColor Q3DTheme::gridLineColor() const
{
    return d_ptr->m_gridLineColor;
}

/*!
 * \property Q3DTheme::singleHighlightColor
 *
 * Highlight color for a highlighted object. Used if \l{Q3DBars::selectionMode}{selectionMode} has
 * \c QAbstract3DGraph::SelectionItem flag set.
 */
void Q3DTheme::setSingleHighlightColor(const QColor &color)
{
    d_ptr->m_dirtyBits.singleHighlightColorDirty = true;
    if (d_ptr->m_singleHighlightColor != color) {
        d_ptr->m_singleHighlightColor = color;
        emit singleHighlightColorChanged(color);
    }
}

QColor Q3DTheme::singleHighlightColor() const
{
    return d_ptr->m_singleHighlightColor;
}

/*!
 * \property Q3DTheme::multiHighlightColor
 *
 * Highlight color for highlighted objects. Used if \l{Q3DBars::selectionMode}{selectionMode} has
 * \c QAbstract3DGraph::SelectionRow or \c QAbstract3DGraph::SelectionColumn flag set.
 */
void Q3DTheme::setMultiHighlightColor(const QColor &color)
{
    d_ptr->m_dirtyBits.multiHighlightColorDirty = true;
    if (d_ptr->m_multiHighlightColor != color) {
        d_ptr->m_multiHighlightColor = color;
        emit multiHighlightColorChanged(color);
    }
}

QColor Q3DTheme::multiHighlightColor() const
{
    return d_ptr->m_multiHighlightColor;
}

/*!
 * \property Q3DTheme::lightColor
 *
 * Color for the specular light defined in Q3DScene.
 */
void Q3DTheme::setLightColor(const QColor &color)
{
    d_ptr->m_dirtyBits.lightColorDirty = true;
    if (d_ptr->m_lightColor != color) {
        d_ptr->m_lightColor = color;
        emit lightColorChanged(color);
        emit d_ptr->needRender();
    }
}

QColor Q3DTheme::lightColor() const
{
    return d_ptr->m_lightColor;
}

/*!
 * \property Q3DTheme::baseGradients
 *
 * List of base gradients to be used for all the objects in the graph, series by series. If there
 * are more series than gradients, gradient list wraps and starts again with the first gradient in
 * the list
 * Has no immediate effect if colorStyle is ColorStyleUniform.
 */
void Q3DTheme::setBaseGradients(const QList<QLinearGradient> &gradients)
{
    if (gradients.size()) {
        d_ptr->m_dirtyBits.baseGradientDirty = true;
        if (d_ptr->m_baseGradients != gradients) {
            d_ptr->m_baseGradients.clear();
            d_ptr->m_baseGradients = gradients;
            emit baseGradientsChanged(gradients);
        }
    } else {
        d_ptr->m_baseGradients.clear();
    }
}

QList<QLinearGradient> Q3DTheme::baseGradients() const
{
    return d_ptr->m_baseGradients;
}

/*!
 * \property Q3DTheme::singleHighlightGradient
 *
 * Highlight gradient for a highlighted object. Used if \l{Q3DBars::selectionMode}{selectionMode}
 * has \c QAbstract3DGraph::SelectionItem flag set.
 */
void Q3DTheme::setSingleHighlightGradient(const QLinearGradient &gradient)
{
    d_ptr->m_dirtyBits.singleHighlightGradientDirty = true;
    if (d_ptr->m_singleHighlightGradient != gradient) {
        d_ptr->m_singleHighlightGradient = gradient;
        emit singleHighlightGradientChanged(gradient);
    }
}

QLinearGradient Q3DTheme::singleHighlightGradient() const
{
    return d_ptr->m_singleHighlightGradient;
}

/*!
 * \property Q3DTheme::multiHighlightGradient
 *
 * Highlight gradient for highlighted objects. Used if \l{Q3DBars::selectionMode}{selectionMode}
 * has \c QAbstract3DGraph::SelectionRow or \c QAbstract3DGraph::SelectionColumn flag set.
 */
void Q3DTheme::setMultiHighlightGradient(const QLinearGradient &gradient)
{
    d_ptr->m_dirtyBits.multiHighlightGradientDirty = true;
    if (d_ptr->m_multiHighlightGradient != gradient) {
        d_ptr->m_multiHighlightGradient = gradient;
        emit multiHighlightGradientChanged(gradient);
    }
}

QLinearGradient Q3DTheme::multiHighlightGradient() const
{
    return d_ptr->m_multiHighlightGradient;
}

/*!
 * \property Q3DTheme::lightStrength
 *
 * Specular light strength for the whole graph. Value must be between 0.0f and 10.0f.
 */
void Q3DTheme::setLightStrength(float strength)
{
    d_ptr->m_dirtyBits.lightStrengthDirty = true;
    if (strength < 0.0f || strength > 10.0f) {
        qWarning("Invalid value. Valid range for lightStrength is between 0.0f and 10.0f");
    } else if (d_ptr->m_lightStrength != strength) {
        d_ptr->m_lightStrength = strength;
        emit lightStrengthChanged(strength);
        emit d_ptr->needRender();
    }
}

float Q3DTheme::lightStrength() const
{
    return d_ptr->m_lightStrength;
}

/*!
 * \property Q3DTheme::ambientLightStrength
 *
 * Ambient light strength for the whole graph. Value must be between 0.0f and 1.0f.
 */
void Q3DTheme::setAmbientLightStrength(float strength)
{
    d_ptr->m_dirtyBits.ambientLightStrengthDirty = true;
    if (strength < 0.0f || strength > 1.0f) {
        qWarning("Invalid value. Valid range for ambientLightStrength is between 0.0f and 1.0f");
    } else if (d_ptr->m_ambientLightStrength != strength) {
        d_ptr->m_ambientLightStrength = strength;
        emit ambientLightStrengthChanged(strength);
        emit d_ptr->needRender();
    }
}

float Q3DTheme::ambientLightStrength() const
{
    return d_ptr->m_ambientLightStrength;
}

/*!
 * \property Q3DTheme::highlightLightStrength
 *
 * Specular light strength for highlighted objects. Value must be between 0.0f and 10.0f.
 */
void Q3DTheme::setHighlightLightStrength(float strength)
{
    d_ptr->m_dirtyBits.highlightLightStrengthDirty = true;
    if (strength < 0.0f || strength > 10.0f) {
        qWarning("Invalid value. Valid range for highlightLightStrength is between 0.0f and 10.0f");
    } else if (d_ptr->m_highlightLightStrength != strength) {
        d_ptr->m_highlightLightStrength = strength;
        emit highlightLightStrengthChanged(strength);
        emit d_ptr->needRender();
    }
}

float Q3DTheme::highlightLightStrength() const
{
    return d_ptr->m_highlightLightStrength;
}

/*!
 * \property Q3DTheme::labelBorderEnabled
 *
 * Set label borders enabled or disabled. Has no effect if labelBackgroundEnabled is \c false.
 */
void Q3DTheme::setLabelBorderEnabled(bool enabled)
{
    d_ptr->m_dirtyBits.labelBorderEnabledDirty = true;
    if (d_ptr->m_labelBorders != enabled) {
        d_ptr->m_labelBorders = enabled;
        emit labelBorderEnabledChanged(enabled);
        emit d_ptr->needRender();
    }
}

bool Q3DTheme::isLabelBorderEnabled() const
{
    return d_ptr->m_labelBorders;
}

/*!
 * \property Q3DTheme::font
 *
 * Set \a font to be used for labels.
 */
void Q3DTheme::setFont(const QFont &font)
{
    d_ptr->m_dirtyBits.fontDirty = true;
    if (d_ptr->m_font != font) {
        d_ptr->m_font = font;
        emit fontChanged(font);
        emit d_ptr->needRender();
    }
}

QFont Q3DTheme::font() const
{
    return d_ptr->m_font;
}

/*!
 * \property Q3DTheme::backgroundEnabled
 *
 * Set background enabled or disabled.
 */
void Q3DTheme::setBackgroundEnabled(bool enabled)
{
    d_ptr->m_dirtyBits.backgroundEnabledDirty = true;
    if (d_ptr->m_backgoundEnabled != enabled) {
        d_ptr->m_backgoundEnabled = enabled;
        emit backgroundEnabledChanged(enabled);
        emit d_ptr->needRender();
    }
}

bool Q3DTheme::isBackgroundEnabled() const
{
    return d_ptr->m_backgoundEnabled;
}

/*!
 * \property Q3DTheme::gridEnabled
 *
 * Set grid lines enabled or disabled.
 */
void Q3DTheme::setGridEnabled(bool enabled)
{
    d_ptr->m_dirtyBits.gridEnabledDirty = true;
    if (d_ptr->m_gridEnabled != enabled) {
        d_ptr->m_gridEnabled = enabled;
        emit gridEnabledChanged(enabled);
        emit d_ptr->needRender();
    }
}

bool Q3DTheme::isGridEnabled() const
{
    return d_ptr->m_gridEnabled;
}

/*!
 * \property Q3DTheme::labelBackgroundEnabled
 *
 * Set label backgrounds enabled or disabled.
 */
void Q3DTheme::setLabelBackgroundEnabled(bool enabled)
{
    d_ptr->m_dirtyBits.labelBackgroundEnabledDirty = true;
    if (d_ptr->m_labelBackground != enabled) {
        d_ptr->m_labelBackground = enabled;
        emit labelBackgroundEnabledChanged(enabled);
        emit d_ptr->needRender();
    }
}

bool Q3DTheme::isLabelBackgroundEnabled() const
{
    return d_ptr->m_labelBackground;
}

/*!
 * \property Q3DTheme::colorStyle
 *
 * The \a style of the graph colors. One of ColorStyle.
 */
void Q3DTheme::setColorStyle(ColorStyle style)
{
    d_ptr->m_dirtyBits.colorStyleDirty = true;
    if (d_ptr->m_colorStyle != style) {
        d_ptr->m_colorStyle = style;
        emit colorStyleChanged(style);
    }
}

Q3DTheme::ColorStyle Q3DTheme::colorStyle() const
{
    return d_ptr->m_colorStyle;
}

/*!
 * \property Q3DTheme::type
 *
 * The type of the theme. The type is automatically set when constructing a theme,
 * but can also be changed later. Changing the theme type will change all other
 * properties of the theme to what the predefined theme specifies.
 * Changing the theme type of the active theme of the graph will also reset all
 * attached series to use the new theme.
 */
void Q3DTheme::setType(Theme themeType)
{
    d_ptr->m_dirtyBits.themeIdDirty = true;
    if (d_ptr->m_themeId != themeType) {
        d_ptr->m_themeId = themeType;
        ThemeManager::setPredefinedPropertiesToTheme(this, themeType);
        emit typeChanged(themeType);
    }
}

Q3DTheme::Theme Q3DTheme::type() const
{
    return d_ptr->m_themeId;
}

// Q3DThemePrivate

Q3DThemePrivate::Q3DThemePrivate(Q3DTheme *q)
    : QObject(0),
      m_themeId(Q3DTheme::ThemeUserDefined),
      m_backgroundColor(Qt::black),
      m_windowColor(Qt::black),
      m_textColor(Qt::white),
      m_textBackgroundColor(Qt::gray),
      m_gridLineColor(Qt::white),
      m_singleHighlightColor(Qt::red),
      m_multiHighlightColor(Qt::blue),
      m_lightColor(Qt::white),
      m_singleHighlightGradient(QLinearGradient(qreal(gradientTextureWidth),
                                                qreal(gradientTextureHeight),
                                                0.0, 0.0)),
      m_multiHighlightGradient(QLinearGradient(qreal(gradientTextureWidth),
                                               qreal(gradientTextureHeight),
                                               0.0, 0.0)),
      m_lightStrength(5.0f),
      m_ambientLightStrength(0.25f),
      m_highlightLightStrength(7.5f),
      m_labelBorders(true),
      m_colorStyle(Q3DTheme::ColorStyleUniform),
      m_font(QFont()),
      m_backgoundEnabled(true),
      m_gridEnabled(true),
      m_labelBackground(true),
      m_isDefaultTheme(false),
      m_forcePredefinedType(true),
      q_ptr(q)
{
    m_baseColors.append(QColor(Qt::black));
    m_baseGradients.append(QLinearGradient(qreal(gradientTextureWidth),
                                           qreal(gradientTextureHeight),
                                           0.0, 0.0));
}

Q3DThemePrivate::~Q3DThemePrivate()
{
}

void Q3DThemePrivate::resetDirtyBits()
{
    m_dirtyBits.ambientLightStrengthDirty = true;
    m_dirtyBits.backgroundColorDirty = true;
    m_dirtyBits.backgroundEnabledDirty = true;
    m_dirtyBits.baseColorDirty = true;
    m_dirtyBits.baseGradientDirty = true;
    m_dirtyBits.colorStyleDirty = true;
    m_dirtyBits.fontDirty = true;
    m_dirtyBits.gridEnabledDirty = true;
    m_dirtyBits.gridLineColorDirty = true;
    m_dirtyBits.highlightLightStrengthDirty = true;
    m_dirtyBits.labelBackgroundColorDirty = true;
    m_dirtyBits.labelBackgroundEnabledDirty = true;
    m_dirtyBits.labelBorderEnabledDirty = true;
    m_dirtyBits.labelTextColorDirty = true;
    m_dirtyBits.lightColorDirty = true;
    m_dirtyBits.lightStrengthDirty = true;
    m_dirtyBits.multiHighlightColorDirty = true;
    m_dirtyBits.multiHighlightGradientDirty = true;
    m_dirtyBits.singleHighlightColorDirty = true;
    m_dirtyBits.singleHighlightGradientDirty = true;
    m_dirtyBits.themeIdDirty = true;
    m_dirtyBits.windowColorDirty = true;
}

bool Q3DThemePrivate::sync(Q3DThemePrivate &other)
{
    bool updateDrawer = false;
    if (m_dirtyBits.ambientLightStrengthDirty) {
        other.q_ptr->setAmbientLightStrength(m_ambientLightStrength);
        m_dirtyBits.ambientLightStrengthDirty = false;
    }
    if (m_dirtyBits.backgroundColorDirty) {
        other.q_ptr->setBackgroundColor(m_backgroundColor);
        m_dirtyBits.backgroundColorDirty = false;
    }
    if (m_dirtyBits.backgroundEnabledDirty) {
        other.q_ptr->setBackgroundEnabled(m_backgoundEnabled);
        m_dirtyBits.backgroundEnabledDirty = false;
    }
    if (m_dirtyBits.baseColorDirty) {
        other.q_ptr->setBaseColors(m_baseColors);
        m_dirtyBits.baseColorDirty = false;
    }
    if (m_dirtyBits.baseGradientDirty) {
        other.q_ptr->setBaseGradients(m_baseGradients);
        m_dirtyBits.baseGradientDirty = false;
    }
    if (m_dirtyBits.colorStyleDirty) {
        other.q_ptr->setColorStyle(m_colorStyle);
        m_dirtyBits.colorStyleDirty = false;
    }
    if (m_dirtyBits.fontDirty) {
        other.q_ptr->setFont(m_font);
        m_dirtyBits.fontDirty = false;
        updateDrawer = true;
    }
    if (m_dirtyBits.gridEnabledDirty) {
        other.q_ptr->setGridEnabled(m_gridEnabled);
        m_dirtyBits.gridEnabledDirty = false;
    }
    if (m_dirtyBits.gridLineColorDirty) {
        other.q_ptr->setGridLineColor(m_gridLineColor);
        m_dirtyBits.gridLineColorDirty = false;
    }
    if (m_dirtyBits.highlightLightStrengthDirty) {
        other.q_ptr->setHighlightLightStrength(m_highlightLightStrength);
        m_dirtyBits.highlightLightStrengthDirty = false;
    }
    if (m_dirtyBits.labelBackgroundColorDirty) {
        other.q_ptr->setLabelBackgroundColor(m_textBackgroundColor);
        m_dirtyBits.labelBackgroundColorDirty = false;
        updateDrawer = true;
    }
    if (m_dirtyBits.labelBackgroundEnabledDirty) {
        other.q_ptr->setLabelBackgroundEnabled(m_labelBackground);
        m_dirtyBits.labelBackgroundEnabledDirty = false;
        updateDrawer = true;
    }
    if (m_dirtyBits.labelBorderEnabledDirty) {
        other.q_ptr->setLabelBorderEnabled(m_labelBorders);
        m_dirtyBits.labelBorderEnabledDirty = false;
        updateDrawer = true;
    }
    if (m_dirtyBits.labelTextColorDirty) {
        other.q_ptr->setLabelTextColor(m_textColor);
        m_dirtyBits.labelTextColorDirty = false;
        updateDrawer = true;
    }
    if (m_dirtyBits.lightColorDirty) {
        other.q_ptr->setLightColor(m_lightColor);
        m_dirtyBits.lightColorDirty = false;
    }
    if (m_dirtyBits.lightStrengthDirty) {
        other.q_ptr->setLightStrength(m_lightStrength);
        m_dirtyBits.lightStrengthDirty = false;
    }
    if (m_dirtyBits.multiHighlightColorDirty) {
        other.q_ptr->setMultiHighlightColor(m_multiHighlightColor);
        m_dirtyBits.multiHighlightColorDirty = false;
    }
    if (m_dirtyBits.multiHighlightGradientDirty) {
        other.q_ptr->setMultiHighlightGradient(m_multiHighlightGradient);
        m_dirtyBits.multiHighlightGradientDirty = false;
    }
    if (m_dirtyBits.singleHighlightColorDirty) {
        other.q_ptr->setSingleHighlightColor(m_singleHighlightColor);
        m_dirtyBits.singleHighlightColorDirty = false;
    }
    if (m_dirtyBits.singleHighlightGradientDirty) {
        other.q_ptr->setSingleHighlightGradient(m_singleHighlightGradient);
        m_dirtyBits.singleHighlightGradientDirty = false;
    }
    if (m_dirtyBits.themeIdDirty) {
        other.m_themeId = m_themeId; // Set directly to avoid a call to ThemeManager's useTheme()
        m_dirtyBits.themeIdDirty = false;
    }
    if (m_dirtyBits.windowColorDirty) {
        other.q_ptr->setWindowColor(m_windowColor);
        m_dirtyBits.windowColorDirty = false;
    }

    return updateDrawer;
}

QT_END_NAMESPACE_DATAVISUALIZATION
