/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "qcustom3dlabel_p.h"
#include "utils_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QCustom3DLabel
 * \inmodule QtDataVisualization
 * \brief The QCustom3DLabel class adds a custom label to a graph.
 * \since QtDataVisualization 1.1
 *
 * The text, font, position, scaling, rotation, and colors of a custom label can
 * be set. In addition, the visibility of the borders and background of the
 * label can be toggled. Colors, borders, and background are determined by the
 * active theme unless set explicitly.
 *
 * \note In scaling, the z-coordinate has no effect. Setting the same x- and
 * y-coordinates retains the original font dimensions.
 *
 * \sa QAbstract3DGraph::addCustomItem()
 */

/*!
 * \qmltype Custom3DLabel
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.1
 * \ingroup datavisualization_qml
 * \instantiates QCustom3DLabel
 * \inherits Custom3DItem
 * \brief Adds a custom label to a graph.
 *
 * The text, font, position, scaling, rotation, and colors of a custom label can
 * be set. In addition, the visibility of the borders and background of the
 * label can be toggled. Colors, borders, and background are determined by the
 * active theme unless set explicitly.
 *
 * \note In scaling, the z-coordinate has no effect. Setting the same x- and
 * y-coordinates retains the original font dimensions.
 */

/*! \qmlproperty string Custom3DLabel::text
 *
 * The text for the label. Rich text is not supported.
 */

/*! \qmlproperty font Custom3DLabel::font
 *
 * The font to be used for the label. Defaults to \c{Font {family: "Arial"; pointSize: 20}}.
 * Special formatting (for example, outlined) is not supported.
 */

/*! \qmlproperty color Custom3DLabel::textColor
 *
 * The color for the label text. Also affects label border, if enabled. Defaults to \c{"white"}.
 *
 * \sa borderEnabled
 */

/*! \qmlproperty color Custom3DLabel::backgroundColor
 *
 * The color for the label background, if enabled. Defaults to \c{"gray"}.
 *
 * \sa backgroundEnabled
 */

/*! \qmlproperty bool Custom3DLabel::backgroundEnabled
 *
 * Defines whether the label background is enabled. If set to \c{false},
 * backgroundColor has no effect. Defaults to \c{true}.
 */

/*! \qmlproperty bool Custom3DLabel::borderEnabled
 *
 * Defines whether label borders are enabled. Defaults to \c{true}.
 */

/*! \qmlproperty bool Custom3DLabel::facingCamera
 *
 * Defines whether the label will always face the camera. Defaults to \c{false}.
 * If set to \c{true}, \l {QCustom3DItem::}{rotation} has no effect.
 */

/*!
 * Constructs a custom 3D label with the given \a parent.
 */
QCustom3DLabel::QCustom3DLabel(QObject *parent) :
    QCustom3DItem(new QCustom3DLabelPrivate(this), parent)
{
}

/*!
 * Constructs a custom 3D label with the given \a text, \a font, \a position, \a scaling,
 * \a rotation, and optional \a parent.
 *
 * \note Setting the same x- and y-coordinates for \a scaling retains the
 * original font dimensions.
 */
QCustom3DLabel::QCustom3DLabel(const QString &text, const QFont &font,
                               const QVector3D &position, const QVector3D &scaling,
                               const QQuaternion &rotation, QObject *parent) :
    QCustom3DItem(new QCustom3DLabelPrivate(this, text, font, position, scaling, rotation),
                  parent)
{
}

/*!
 * Deletes the custom 3D label.
 */
QCustom3DLabel::~QCustom3DLabel()
{
}

/*! \property QCustom3DLabel::text
 *
 * \brief The text for the label.
 *
 * Rich text is not supported.
 */
void QCustom3DLabel::setText(const QString &text)
{
    if (dptr()->m_text != text) {
        dptr()->m_text = text;
        dptr()->handleTextureChange();
        emit textChanged(text);
        emit dptr()->needUpdate();
    }
}

QString QCustom3DLabel::text() const
{
    return dptrc()->m_text;
}

/*! \property QCustom3DLabel::font
 *
 * \brief The font to be used for the label.
 *
 * Defaults to \c{QFont("Arial", 20)}. Special formatting
 * (for example, outlined) is not supported.
 */
void QCustom3DLabel::setFont(const QFont &font)
{
    if (dptr()->m_font != font) {
        dptr()->m_font = font;
        dptr()->handleTextureChange();
        emit fontChanged(font);
        emit dptr()->needUpdate();
    }
}

QFont QCustom3DLabel::font() const
{
    return dptrc()->m_font;
}

/*! \property QCustom3DLabel::textColor
 *
 * \brief The color for the label text.
 *
 * Also affects the label border, if enabled. Defaults to \c{Qt::white}.
 *
 * \sa borderEnabled
 */
void QCustom3DLabel::setTextColor(const QColor &color)
{
    if (dptr()->m_txtColor != color) {
        dptr()->m_txtColor = color;
        dptr()->m_customVisuals = true;
        dptr()->handleTextureChange();
        emit textColorChanged(color);
        emit dptr()->needUpdate();
    }
}

QColor QCustom3DLabel::textColor() const
{
    return dptrc()->m_txtColor;
}

/*! \property QCustom3DLabel::backgroundColor
 *
 * \brief The color for the label background, if enabled.
 *
 * Defaults to \c{Qt::gray}.
 *
 * \sa backgroundEnabled
 */
void QCustom3DLabel::setBackgroundColor(const QColor &color)
{
    if (dptr()->m_bgrColor != color) {
        dptr()->m_bgrColor = color;
        dptr()->m_customVisuals = true;
        dptr()->handleTextureChange();
        emit backgroundColorChanged(color);
        emit dptr()->needUpdate();
    }
}

QColor QCustom3DLabel::backgroundColor() const
{
    return dptrc()->m_bgrColor;
}

/*! \property QCustom3DLabel::borderEnabled
 *
 * \brief Whether label borders are enabled.
 *
 * Defaults to \c{true}.
 */
void QCustom3DLabel::setBorderEnabled(bool enabled)
{
    if (dptr()->m_borders != enabled) {
        dptr()->m_borders = enabled;
        dptr()->m_customVisuals = true;
        dptr()->handleTextureChange();
        emit borderEnabledChanged(enabled);
        emit dptr()->needUpdate();
    }
}

bool QCustom3DLabel::isBorderEnabled() const
{
    return dptrc()->m_borders;
}

/*! \property QCustom3DLabel::backgroundEnabled
 *
 * \brief Whether the label background is enabled.
 *
 * If set to \c{false}, backgroundColor() has no effect. Defaults
 * to \c{true}.
 */
void QCustom3DLabel::setBackgroundEnabled(bool enabled)
{
    if (dptr()->m_background != enabled) {
        dptr()->m_background = enabled;
        dptr()->m_customVisuals = true;
        dptr()->handleTextureChange();
        emit backgroundEnabledChanged(enabled);
        emit dptr()->needUpdate();
    }
}

bool QCustom3DLabel::isBackgroundEnabled() const
{
    return dptrc()->m_background;
}

/*! \property QCustom3DLabel::facingCamera
 *
 * \brief Whether the label will always face the camera.
 *
 * Defaults to \c{false}. If set to \c{true}, rotation()
 * has no effect.
 */
void QCustom3DLabel::setFacingCamera(bool enabled)
{
    if (dptr()->m_facingCamera != enabled) {
        dptr()->m_facingCamera = enabled;
        dptr()->m_facingCameraDirty = true;
        emit facingCameraChanged(enabled);
        emit dptr()->needUpdate();
    }
}

bool QCustom3DLabel::isFacingCamera() const
{
    return dptrc()->m_facingCamera;
}

/*!
 * \internal
 */
QCustom3DLabelPrivate *QCustom3DLabel::dptr()
{
    return static_cast<QCustom3DLabelPrivate *>(d_ptr.data());
}

/*!
 * \internal
 */
const QCustom3DLabelPrivate *QCustom3DLabel::dptrc() const
{
    return static_cast<const QCustom3DLabelPrivate *>(d_ptr.data());
}

QCustom3DLabelPrivate::QCustom3DLabelPrivate(QCustom3DLabel *q) :
    QCustom3DItemPrivate(q),
    m_font(QFont(QStringLiteral("Arial"), 20)),
    m_bgrColor(Qt::gray),
    m_txtColor(Qt::white),
    m_background(true),
    m_borders(true),
    m_facingCamera(false),
    m_customVisuals(false),
    m_facingCameraDirty(false)
{
    m_isLabelItem = true;
    m_shadowCasting = false;
    m_meshFile = QStringLiteral(":/defaultMeshes/plane");
    createTextureImage();
}

QCustom3DLabelPrivate::QCustom3DLabelPrivate(QCustom3DLabel *q, const QString &text,
                                             const QFont &font, const QVector3D &position,
                                             const QVector3D &scaling,
                                             const QQuaternion &rotation) :
    QCustom3DItemPrivate(q, QStringLiteral(":/defaultMeshes/plane"), position, scaling, rotation),
    m_text(text),
    m_font(font),
    m_bgrColor(Qt::gray),
    m_txtColor(Qt::white),
    m_background(true),
    m_borders(true),
    m_facingCamera(false),
    m_customVisuals(false),
    m_facingCameraDirty(false)
{
    m_isLabelItem = true;
    m_shadowCasting = false;
    createTextureImage();
}

QCustom3DLabelPrivate::~QCustom3DLabelPrivate()
{
}

void QCustom3DLabelPrivate::resetDirtyBits()
{
    QCustom3DItemPrivate::resetDirtyBits();
    m_facingCameraDirty = false;
}

void QCustom3DLabelPrivate::createTextureImage()
{
    createTextureImage(m_bgrColor, m_txtColor, m_background, m_borders);
}

void QCustom3DLabelPrivate::createTextureImage(const QColor &bgrColor, const QColor &txtColor,
                                               bool background, bool borders)
{
    m_textureImage = Utils::printTextToImage(m_font, m_text, bgrColor, txtColor, background,
                                             borders, 0);
}

void QCustom3DLabelPrivate::handleTextureChange()
{
    createTextureImage();
    m_dirtyBits.textureDirty = true;
    if (!m_textureFile.isEmpty()) {
        m_textureFile.clear();
        emit q_ptr->textureFileChanged(m_textureFile);
    }
}

QT_END_NAMESPACE_DATAVISUALIZATION
