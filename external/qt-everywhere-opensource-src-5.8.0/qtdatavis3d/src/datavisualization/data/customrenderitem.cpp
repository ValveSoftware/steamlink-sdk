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

#include "customrenderitem_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

CustomRenderItem::CustomRenderItem()
    : AbstractRenderItem(),
      m_texture(0),
      m_positionAbsolute(false),
      m_scalingAbsolute(true),
      m_object(0),
      m_needBlend(true),
      m_visible(true),
      m_valid(true),
      m_index(0),
      m_shadowCasting(false),
      m_isFacingCamera(false),
      m_item(0),
      m_renderer(0),
      m_labelItem(false),
      m_textureWidth(0),
      m_textureHeight(0),
      m_textureDepth(0),
      m_isVolume(false),
      m_textureFormat(QImage::Format_ARGB32),
      m_sliceIndexX(-1),
      m_sliceIndexY(-1),
      m_sliceIndexZ(-1),
      m_alphaMultiplier(1.0f),
      m_preserveOpacity(true),
      m_useHighDefShader(true),
      m_drawSlices(false),
      m_drawSliceFrames(false)

{
}

CustomRenderItem::~CustomRenderItem()
{
    ObjectHelper::releaseObjectHelper(m_renderer, m_object);
}

void CustomRenderItem::setMesh(const QString &meshFile)
{
    ObjectHelper::resetObjectHelper(m_renderer, m_object, meshFile);
}

void CustomRenderItem::setColorTable(const QVector<QRgb> &colors)
{
    m_colorTable.resize(256);
    for (int i = 0; i < 256; i++) {
        if (i < colors.size()) {
            const QRgb &rgb = colors.at(i);
            m_colorTable[i] = QVector4D(float(qRed(rgb)) / 255.0f,
                                        float(qGreen(rgb)) / 255.0f,
                                        float(qBlue(rgb)) / 255.0f,
                                        float(qAlpha(rgb)) / 255.0f);
        } else {
            m_colorTable[i] = QVector4D(0.0f, 0.0f, 0.0f, 0.0f);
        }
    }
}

void CustomRenderItem::setMinBounds(const QVector3D &bounds)
{
    m_minBounds = bounds;
    m_minBoundsNormal = m_minBounds;
    m_minBoundsNormal.setY(-m_minBoundsNormal.y());
    m_minBoundsNormal.setZ(-m_minBoundsNormal.z());
    m_minBoundsNormal = 0.5f * (m_minBoundsNormal + oneVector);
}

void CustomRenderItem::setMaxBounds(const QVector3D &bounds)
{
    m_maxBounds = bounds;
    m_maxBoundsNormal = m_maxBounds;
    m_maxBoundsNormal.setY(-m_maxBoundsNormal.y());
    m_maxBoundsNormal.setZ(-m_maxBoundsNormal.z());
    m_maxBoundsNormal = 0.5f * (m_maxBoundsNormal + oneVector);
}

void CustomRenderItem::setSliceFrameColor(const QColor &color)
{
    const QRgb &rgb = color.rgba();
    m_sliceFrameColor = QVector4D(float(qRed(rgb)) / 255.0f,
                                  float(qGreen(rgb)) / 255.0f,
                                  float(qBlue(rgb)) / 255.0f,
                                  float(1.0f)); // Alpha not supported for frames
}

QT_END_NAMESPACE_DATAVISUALIZATION
