/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#include "qquickandroid9patch_p.h"
#include <qquickwindow.h>
#include <qsgnode.h>

QT_BEGIN_NAMESPACE

class QQuickAndroid9PatchNode1 : public QSGGeometryNode
{
public:
    QQuickAndroid9PatchNode1();
    ~QQuickAndroid9PatchNode1();

    void initialize(QSGTexture *texture, const QRectF &bounds, const QSize &sourceSize,
                    const QQuickAndroid9PatchDivs1 &xDivs, const QQuickAndroid9PatchDivs1 &yDivs);

private:
    QSGGeometry m_geometry;
    QSGTextureMaterial m_material;
};

QQuickAndroid9PatchNode1::QQuickAndroid9PatchNode1()
    : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
{
    m_geometry.setDrawingMode(GL_TRIANGLES);
    setGeometry(&m_geometry);
    setMaterial(&m_material);
}

QQuickAndroid9PatchNode1::~QQuickAndroid9PatchNode1()
{
    delete m_material.texture();
}

void QQuickAndroid9PatchNode1::initialize(QSGTexture *texture, const QRectF &bounds, const QSize &sourceSize,
                                         const QQuickAndroid9PatchDivs1 &xDivs, const QQuickAndroid9PatchDivs1 &yDivs)
{
    delete m_material.texture();
    m_material.setTexture(texture);

    const int xlen = xDivs.data.size();
    const int ylen = yDivs.data.size();

    if (xlen > 0 && ylen > 0) {
        const int quads = (xlen - 1) * (ylen - 1);
        static const int verticesPerQuad = 6;
        m_geometry.allocate(xlen * ylen, verticesPerQuad * quads);

        QSGGeometry::TexturedPoint2D *vertices = m_geometry.vertexDataAsTexturedPoint2D();
        QVector<qreal> xCoords = xDivs.coordsForSize(bounds.width());
        QVector<qreal> yCoords = yDivs.coordsForSize(bounds.height());
        for (int y = 0; y < ylen; ++y) {
            for (int x = 0; x < xlen; ++x, ++vertices)
                vertices->set(xCoords[x], yCoords[y], xDivs.data[x] / sourceSize.width(),
                                                      yDivs.data[y] / sourceSize.height());
        }

        quint16 *indices = m_geometry.indexDataAsUShort();
        int n = quads;
        for (int q = 0; n--; ++q) {
            if ((q + 1) % xlen == 0) // next row
                ++q;
            // Bottom-left half quad triangle
            indices[0] = q;
            indices[1] = q + xlen;
            indices[2] = q + xlen + 1;

            // Top-right half quad triangle
            indices[3] = q;
            indices[4] = q + xlen + 1;
            indices[5] = q + 1;

            indices += verticesPerQuad;
        }
    }

    markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
}

QVector<qreal> QQuickAndroid9PatchDivs1::coordsForSize(qreal size) const
{
    // n = number of stretchable sections
    // We have to compensate when adding 0 and/or
    // the source image width to the divs vector.
    const int l = data.size();
    const int n = (inverted ? l - 1 : l) >> 1;
    const qreal stretchAmount = (size - data.last()) / n;

    QVector<qreal> coords;
    coords.reserve(l);
    coords.append(0);

    bool stretch = !inverted;
    for (int i = 1; i < l; ++i) {
        qreal advance = data[i] - data[i - 1];
        if (stretch)
            advance += stretchAmount;
        coords.append(coords.last() + advance);

        stretch = !stretch;
    }

    return coords;
}

void QQuickAndroid9PatchDivs1::fill(const QVariantList &divs, qreal size)
{
    if (!data.isEmpty())
        return;

    inverted = divs.isEmpty() || divs.first().toInt() != 0;
    // Reserve an extra item in case we need to add the image width/height
    if (inverted) {
        data.reserve(divs.size() + 2);
        data.append(0);
    } else {
        data.reserve(divs.size() + 1);
    }

    for (const QVariant &div : divs)
        data.append(div.toReal());

    data.append(size);
}

void QQuickAndroid9PatchDivs1::clear()
{
    data.clear();
}

QQuickAndroid9Patch1::QQuickAndroid9Patch1(QQuickItem *parent) : QQuickItem(parent)
{
    connect(this, SIGNAL(widthChanged()), this, SLOT(updateDivs()));
    connect(this, SIGNAL(heightChanged()), this, SLOT(updateDivs()));
}

QQuickAndroid9Patch1::~QQuickAndroid9Patch1()
{
}

QUrl QQuickAndroid9Patch1::source() const
{
    return m_source;
}

void QQuickAndroid9Patch1::setSource(const QUrl &source)
{
    if (m_source != source) {
        m_source = source;
        m_xDivs.clear();
        m_yDivs.clear();
        loadImage();
        m_sourceSize = m_image.size();

        emit sourceChanged(source);
        emit sourceSizeChanged(m_sourceSize);
    }
}

QVariantList QQuickAndroid9Patch1::xDivs() const
{
    return m_xVars;
}

void QQuickAndroid9Patch1::setXDivs(const QVariantList &divs)
{
    if (m_xVars != divs) {
        m_xVars = divs;

        m_xDivs.clear();
        updateDivs();
        emit xDivsChanged(divs);
    }
}

QVariantList QQuickAndroid9Patch1::yDivs() const
{
    return m_yVars;
}

void QQuickAndroid9Patch1::setYDivs(const QVariantList &divs)
{
    if (m_yVars != divs) {
        m_yVars = divs;

        m_yDivs.clear();
        updateDivs();
        emit yDivsChanged(divs);
    }
}

QSize QQuickAndroid9Patch1::sourceSize() const
{
    return m_sourceSize;
}

void QQuickAndroid9Patch1::classBegin()
{
    QQuickItem::classBegin();
}

void QQuickAndroid9Patch1::componentComplete()
{
    QQuickItem::componentComplete();
    loadImage();
}

QSGNode *QQuickAndroid9Patch1::updatePaintNode(QSGNode *node, UpdatePaintNodeData *data)
{
    Q_UNUSED(data);

    if (m_image.isNull()) {
        delete node;
        return 0;
    }

    QQuickAndroid9PatchNode1 *patchNode = static_cast<QQuickAndroid9PatchNode1 *>(node);
    if (!patchNode)
        patchNode = new QQuickAndroid9PatchNode1;

#ifdef QSG_RUNTIME_DESCRIPTION
    qsgnode_set_description(patchNode, QString::fromLatin1("Android9Patch: '%1'").arg(m_source.toString()));
#endif

    QSGTexture *texture = window()->createTextureFromImage(m_image);
    patchNode->initialize(texture, boundingRect(), m_image.size(), m_xDivs, m_yDivs);
    return patchNode;
}

void QQuickAndroid9Patch1::loadImage()
{
    if (!isComponentComplete())
        return;

    if (m_source.isEmpty())
        m_image = QImage();
    else
        m_image = QImage(m_source.toLocalFile());

    setFlag(QQuickItem::ItemHasContents, !m_image.isNull());
    setImplicitSize(m_image.width(), m_image.height());

    updateDivs();
}

void QQuickAndroid9Patch1::updateDivs()
{
    if (!isComponentComplete() || m_image.isNull() || width() <= 0 || height() <= 0)
        return;

    m_xDivs.fill(m_xVars, m_image.width());
    m_yDivs.fill(m_yVars, m_image.height());

    update();
}

QT_END_NAMESPACE
