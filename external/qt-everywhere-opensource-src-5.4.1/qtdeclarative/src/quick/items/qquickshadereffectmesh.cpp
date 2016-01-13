/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickshadereffectmesh_p.h"
#include <QtQuick/qsggeometry.h>
#include "qquickshadereffect_p.h"

QT_BEGIN_NAMESPACE

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
    connect(this, SIGNAL(resolutionChanged()), this, SIGNAL(geometryChanged()));
}

QSGGeometry *QQuickGridMesh::updateGeometry(QSGGeometry *geometry, const QVector<QByteArray> &attributes, const QRectF &srcRect, const QRectF &dstRect)
{
    int vmesh = m_resolution.height();
    int hmesh = m_resolution.width();
    int attrCount = attributes.count();

    int positionIndex = attributes.indexOf(qtPositionAttributeName());
    int texCoordIndex = attributes.indexOf(qtTexCoordAttributeName());

    if (!geometry) {
        switch (attrCount) {
        case 0:
            m_log = QLatin1String("Error: No attributes specified.");
            return 0;
        case 1:
            if (positionIndex != 0) {
                m_log = QLatin1String("Error: Missing \'");
                m_log += QLatin1String(qtPositionAttributeName());
                m_log += QLatin1String("\' attribute.\n");
                return 0;
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
                return 0;
            }
            break;
        default:
            m_log = QLatin1String("Error: Too many attributes specified.");
            return 0;
        }

        geometry = new QSGGeometry(attrCount == 1
                                   ? QSGGeometry::defaultAttributes_Point2D()
                                   : QSGGeometry::defaultAttributes_TexturedPoint2D(),
                                   (vmesh + 1) * (hmesh + 1), vmesh * 2 * (hmesh + 2),
                                   GL_UNSIGNED_SHORT);

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
                if (ia == positionIndex) {
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
}

QSize QQuickGridMesh::resolution() const
{
    return m_resolution;
}

QT_END_NAMESPACE
