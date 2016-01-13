/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#include "qquickandroid9patch_p.h"
#include <qquickwindow.h>
#include <qsgnode.h>

QT_BEGIN_NAMESPACE

class QQuickAndroid9PatchNode : public QSGGeometryNode
{
public:
    QQuickAndroid9PatchNode();
    ~QQuickAndroid9PatchNode();

    void initialize(QSGTexture *texture, const QRectF &bounds, const QSize &sourceSize,
                    const QQuickAndroid9PatchDivs &xDivs, const QQuickAndroid9PatchDivs &yDivs);

private:
    QSGGeometry m_geometry;
    QSGTextureMaterial m_material;
};

QQuickAndroid9PatchNode::QQuickAndroid9PatchNode()
    : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
{
    m_geometry.setDrawingMode(GL_TRIANGLES);
    setGeometry(&m_geometry);
    setMaterial(&m_material);
}

QQuickAndroid9PatchNode::~QQuickAndroid9PatchNode()
{
    delete m_material.texture();
}

void QQuickAndroid9PatchNode::initialize(QSGTexture *texture, const QRectF &bounds, const QSize &sourceSize,
                                         const QQuickAndroid9PatchDivs &xDivs, const QQuickAndroid9PatchDivs &yDivs)
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

QVector<qreal> QQuickAndroid9PatchDivs::coordsForSize(qreal size) const
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

void QQuickAndroid9PatchDivs::fill(const QVariantList &divs, qreal size)
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

    foreach (const QVariant &div, divs)
        data.append(div.toReal());

    data.append(size);
}

void QQuickAndroid9PatchDivs::clear()
{
    data.clear();
}

QQuickAndroid9Patch::QQuickAndroid9Patch(QQuickItem *parent) : QQuickItem(parent)
{
    connect(this, SIGNAL(widthChanged()), this, SLOT(updateDivs()));
    connect(this, SIGNAL(heightChanged()), this, SLOT(updateDivs()));
}

QQuickAndroid9Patch::~QQuickAndroid9Patch()
{
}

QUrl QQuickAndroid9Patch::source() const
{
    return m_source;
}

void QQuickAndroid9Patch::setSource(const QUrl &source)
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

QVariantList QQuickAndroid9Patch::xDivs() const
{
    return m_xVars;
}

void QQuickAndroid9Patch::setXDivs(const QVariantList &divs)
{
    if (m_xVars != divs) {
        m_xVars = divs;

        m_xDivs.clear();
        updateDivs();
        emit xDivsChanged(divs);
    }
}

QVariantList QQuickAndroid9Patch::yDivs() const
{
    return m_yVars;
}

void QQuickAndroid9Patch::setYDivs(const QVariantList &divs)
{
    if (m_yVars != divs) {
        m_yVars = divs;

        m_yDivs.clear();
        updateDivs();
        emit yDivsChanged(divs);
    }
}

QSize QQuickAndroid9Patch::sourceSize() const
{
    return m_sourceSize;
}

void QQuickAndroid9Patch::classBegin()
{
    QQuickItem::classBegin();
}

void QQuickAndroid9Patch::componentComplete()
{
    QQuickItem::componentComplete();
    loadImage();
}

QSGNode *QQuickAndroid9Patch::updatePaintNode(QSGNode *node, UpdatePaintNodeData *data)
{
    Q_UNUSED(data);

    if (m_image.isNull()) {
        delete node;
        return 0;
    }

    QQuickAndroid9PatchNode *patchNode = static_cast<QQuickAndroid9PatchNode *>(node);
    if (!patchNode)
        patchNode = new QQuickAndroid9PatchNode;

#ifdef QSG_RUNTIME_DESCRIPTION
    qsgnode_set_description(patchNode, QString::fromLatin1("Android9Patch: '%1'").arg(m_source.toString()));
#endif

    QSGTexture *texture = window()->createTextureFromImage(m_image);
    patchNode->initialize(texture, boundingRect(), m_image.size(), m_xDivs, m_yDivs);
    return patchNode;
}

void QQuickAndroid9Patch::loadImage()
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

void QQuickAndroid9Patch::updateDivs()
{
    if (!isComponentComplete() || m_image.isNull() || width() <= 0 || height() <= 0)
        return;

    m_xDivs.fill(m_xVars, m_image.width());
    m_yDivs.fill(m_yVars, m_image.height());

    update();
}

QT_END_NAMESPACE
