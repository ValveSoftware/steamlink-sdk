/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickshadereffectmesh_p.h"
#include <QtQuick/qsggeometry.h>
#include "qquickshadereffect_p.h"
#include "qquickscalegrid_p_p.h"
#include "qquickborderimage_p_p.h"
#include <QtQuick/private/qsgbasicinternalimagenode_p.h>

QT_BEGIN_NAMESPACE

static const char qt_position_attribute_name[] = "qt_Vertex";
static const char qt_texcoord_attribute_name[] = "qt_MultiTexCoord0";

const char *qtPositionAttributeName()
{
    return qt_position_attribute_name;
}

const char *qtTexCoordAttributeName()
{
    return qt_texcoord_attribute_name;
}

QQuickShaderEffectMesh::QQuickShaderEffectMesh(QObject *parent)
    : QObject(parent)
{
}

/*!
    \qmltype GridMesh
    \instantiates QQuickGridMesh
    \inqmlmodule QtQuick
    \since 5.0
    \ingroup qtquick-effects
    \brief Defines a mesh with vertices arranged in a grid

    GridMesh defines a rectangular mesh consisting of vertices arranged in an
    evenly spaced grid. It is used to generate \l{QSGGeometry}{geometry}.
    The grid resolution is specified with the \l resolution property.
*/

QQuickGridMesh::QQuickGridMesh(QObject *parent)
    : QQuickShaderEffectMesh(parent)
    , m_resolution(1, 1)
{
}

bool QQuickGridMesh::validateAttributes(const QVector<QByteArray> &attributes, int *posIndex)
{
    const int attrCount = attributes.count();
    int positionIndex = attributes.indexOf(qtPositionAttributeName());
    int texCoordIndex = attributes.indexOf(qtTexCoordAttributeName());

    switch (attrCount) {
    case 0:
        m_log = QLatin1String("Error: No attributes specified.");
        return false;
    case 1:
        if (positionIndex != 0) {
            m_log = QLatin1String("Error: Missing \'");
            m_log += QLatin1String(qtPositionAttributeName());
            m_log += QLatin1String("\' attribute.\n");
            return false;
        }
        break;
    case 2:
        if (positionIndex == -1 || texCoordIndex == -1) {
            m_log.clear();
            if (positionIndex == -1) {
                m_log = QLatin1String("Error: Missing \'");
                m_log += QLatin1String(qtPositionAttributeName());
                m_log += QLatin1String("\' attribute.\n");
            }
            if (texCoordIndex == -1) {
                m_log += QLatin1String("Error: Missing \'");
                m_log += QLatin1String(qtTexCoordAttributeName());
                m_log += QLatin1String("\' attribute.\n");
            }
            return false;
        }
        break;
    default:
        m_log = QLatin1String("Error: Too many attributes specified.");
        return false;
    }

    if (posIndex)
        *posIndex = positionIndex;

    return true;
}

QSGGeometry *QQuickGridMesh::updateGeometry(QSGGeometry *geometry, int attrCount, int posIndex,
                                            const QRectF &srcRect, const QRectF &dstRect)
{
    int vmesh = m_resolution.height();
    int hmesh = m_resolution.width();

    if (!geometry) {
        Q_ASSERT(attrCount == 1 || attrCount == 2);
        geometry = new QSGGeometry(attrCount == 1
                                   ? QSGGeometry::defaultAttributes_Point2D()
                                   : QSGGeometry::defaultAttributes_TexturedPoint2D(),
                                   (vmesh + 1) * (hmesh + 1), vmesh * 2 * (hmesh + 2),
                                   QSGGeometry::UnsignedShortType);

    } else {
        geometry->allocate((vmesh + 1) * (hmesh + 1), vmesh * 2 * (hmesh + 2));
    }

    QSGGeometry::Point2D *vdata = static_cast<QSGGeometry::Point2D *>(geometry->vertexData());

    for (int iy = 0; iy <= vmesh; ++iy) {
        float fy = iy / float(vmesh);
        float y = float(dstRect.top()) + fy * float(dstRect.height());
        float ty = float(srcRect.top()) + fy * float(srcRect.height());
        for (int ix = 0; ix <= hmesh; ++ix) {
            float fx = ix / float(hmesh);
            for (int ia = 0; ia < attrCount; ++ia) {
                if (ia == posIndex) {
                    vdata->x = float(dstRect.left()) + fx * float(dstRect.width());
                    vdata->y = y;
                    ++vdata;
                } else {
                    vdata->x = float(srcRect.left()) + fx * float(srcRect.width());
                    vdata->y = ty;
                    ++vdata;
                }
            }
        }
    }

    quint16 *indices = (quint16 *)geometry->indexDataAsUShort();
    int i = 0;
    for (int iy = 0; iy < vmesh; ++iy) {
        *(indices++) = i + hmesh + 1;
        for (int ix = 0; ix <= hmesh; ++ix, ++i) {
            *(indices++) = i + hmesh + 1;
            *(indices++) = i;
        }
        *(indices++) = i - 1;
    }

    return geometry;
}

/*!
    \qmlproperty size QtQuick::GridMesh::resolution

    This property holds the grid resolution. The resolution's width and height
    specify the number of cells or spacings between vertices horizontally and
    vertically respectively. The minimum and default is 1x1, which corresponds
    to four vertices in total, one in each corner.
    For non-linear vertex transformations, you probably want to set the
    resolution higher.

    \table
    \row
    \li \image declarative-gridmesh.png
    \li \qml
        import QtQuick 2.0

        ShaderEffect {
            width: 200
            height: 200
            mesh: GridMesh {
                resolution: Qt.size(20, 20)
            }
            property variant source: Image {
                source: "qt-logo.png"
                sourceSize { width: 200; height: 200 }
            }
            vertexShader: "
                uniform highp mat4 qt_Matrix;
                attribute highp vec4 qt_Vertex;
                attribute highp vec2 qt_MultiTexCoord0;
                varying highp vec2 qt_TexCoord0;
                uniform highp float width;
                void main() {
                    highp vec4 pos = qt_Vertex;
                    highp float d = .5 * smoothstep(0., 1., qt_MultiTexCoord0.y);
                    pos.x = width * mix(d, 1.0 - d, qt_MultiTexCoord0.x);
                    gl_Position = qt_Matrix * pos;
                    qt_TexCoord0 = qt_MultiTexCoord0;
                }"
        }
        \endqml
    \endtable
*/

void QQuickGridMesh::setResolution(const QSize &res)
{
    if (res == m_resolution)
        return;
    if (res.width() < 1 || res.height() < 1) {
        return;
    }
    m_resolution = res;
    emit resolutionChanged();
    emit geometryChanged();
}

QSize QQuickGridMesh::resolution() const
{
    return m_resolution;
}

/*!
    \qmltype BorderImageMesh
    \instantiates QQuickBorderImageMesh
    \inqmlmodule QtQuick
    \since 5.8
    \ingroup qtquick-effects
    \brief Defines a mesh with vertices arranged like those of a BorderImage.

    BorderImageMesh provides BorderImage-like capabilities to a ShaderEffect
    without the need for a potentially costly ShaderEffectSource.

    The following are functionally equivalent:
    \qml
    BorderImage {
        id: borderImage
        border {
            left: 10
            right: 10
            top: 10
            bottom: 10
        }
        source: "myImage.png"
        visible: false
    }
    ShaderEffectSource {
        id: effectSource
        sourceItem: borderImage
        visible: false
    }
    ShaderEffect {
        property var source: effectSource
        ...
    }
    \endqml

    \qml
    Image {
        id: image
        source: "myImage.png"
        visible: false
    }
    ShaderEffect {
        property var source: image
        mesh: BorderImageMesh {
            border {
                left: 10
                right: 10
                top: 10
                bottom: 10
            }
            size: image.sourceSize
        }
        ...
    }
    \endqml

    But the BorderImageMesh version can typically be better optimized.
*/
QQuickBorderImageMesh::QQuickBorderImageMesh(QObject *parent)
    : QQuickShaderEffectMesh(parent), m_border(new QQuickScaleGrid(this)),
      m_horizontalTileMode(QQuickBorderImageMesh::Stretch),
      m_verticalTileMode(QQuickBorderImageMesh::Stretch)
{
}

bool QQuickBorderImageMesh::validateAttributes(const QVector<QByteArray> &attributes, int *posIndex)
{
    Q_UNUSED(attributes);
    Q_UNUSED(posIndex);
    return true;
}

QSGGeometry *QQuickBorderImageMesh::updateGeometry(QSGGeometry *geometry, int attrCount, int posIndex,
                                                   const QRectF &srcRect, const QRectF &rect)
{
    Q_UNUSED(attrCount);
    Q_UNUSED(posIndex);

    QRectF innerSourceRect;
    QRectF targetRect;
    QRectF innerTargetRect;
    QRectF subSourceRect;

    QQuickBorderImagePrivate::calculateRects(m_border, m_size, rect.size(), m_horizontalTileMode, m_verticalTileMode,
                                             1, &targetRect, &innerTargetRect, &innerSourceRect, &subSourceRect);

    QRectF sourceRect = srcRect;
    QRectF modifiedInnerSourceRect(sourceRect.x() + innerSourceRect.x() * sourceRect.width(),
                                   sourceRect.y() + innerSourceRect.y() * sourceRect.height(),
                                   innerSourceRect.width() * sourceRect.width(),
                                   innerSourceRect.height() * sourceRect.height());

    geometry = QSGBasicInternalImageNode::updateGeometry(targetRect, innerTargetRect, sourceRect,
                                                         modifiedInnerSourceRect, subSourceRect, geometry);

    return geometry;
}

/*!
    \qmlpropertygroup QtQuick::BorderImageMesh::border
    \qmlproperty int QtQuick::BorderImageMesh::border.left
    \qmlproperty int QtQuick::BorderImageMesh::border.right
    \qmlproperty int QtQuick::BorderImageMesh::border.top
    \qmlproperty int QtQuick::BorderImageMesh::border.bottom

    The 4 border lines (2 horizontal and 2 vertical) break the image into 9 sections,
    as shown below:

    \image declarative-scalegrid.png

    Each border line (left, right, top, and bottom) specifies an offset in pixels
    from the respective edge of the mesh. By default, each border line has
    a value of 0.

    For example, the following definition sets the bottom line 10 pixels up from
    the bottom of the mesh:

    \qml
    BorderImageMesh {
        border.bottom: 10
        // ...
    }
    \endqml
*/
QQuickScaleGrid *QQuickBorderImageMesh::border()
{
    return m_border;
}

/*!
    \qmlproperty size QtQuick::BorderImageMesh::size

    The base size of the mesh. This generally corresponds to the \l {Image::}{sourceSize}
    of the image being used by the ShaderEffect.
*/
QSize QQuickBorderImageMesh::size() const
{
    return m_size;
}

void QQuickBorderImageMesh::setSize(const QSize &size)
{
    if (size == m_size)
        return;
    m_size = size;
    Q_EMIT sizeChanged();
    Q_EMIT geometryChanged();
}

/*!
    \qmlproperty enumeration QtQuick::BorderImageMesh::horizontalTileMode
    \qmlproperty enumeration QtQuick::BorderImageMesh::verticalTileMode

    This property describes how to repeat or stretch the middle parts of an image.

    \list
    \li BorderImage.Stretch - Scales the image to fit to the available area.
    \li BorderImage.Repeat - Tile the image until there is no more space. May crop the last image.
    \li BorderImage.Round - Like Repeat, but scales the images down to ensure that the last image is not cropped.
    \endlist

    The default tile mode for each property is BorderImage.Stretch.
*/

QQuickBorderImageMesh::TileMode QQuickBorderImageMesh::horizontalTileMode() const
{
    return m_horizontalTileMode;
}

void QQuickBorderImageMesh::setHorizontalTileMode(TileMode t)
{
    if (t == m_horizontalTileMode)
        return;
    m_horizontalTileMode = t;
    Q_EMIT horizontalTileModeChanged();
    Q_EMIT geometryChanged();
}

QQuickBorderImageMesh::TileMode QQuickBorderImageMesh::verticalTileMode() const
{
    return m_verticalTileMode;
}

void QQuickBorderImageMesh::setVerticalTileMode(TileMode t)
{
    if (t == m_verticalTileMode)
        return;

    m_verticalTileMode = t;
    Q_EMIT verticalTileModeChanged();
    Q_EMIT geometryChanged();
}

QT_END_NAMESPACE
