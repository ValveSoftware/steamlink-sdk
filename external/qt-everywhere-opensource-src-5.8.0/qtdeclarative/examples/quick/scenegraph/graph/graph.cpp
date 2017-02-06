/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "graph.h"

#include "noisynode.h"
#include "gridnode.h"
#include "linenode.h"

Graph::Graph()
    : m_samplesChanged(false)
    , m_geometryChanged(false)
{
    setFlag(ItemHasContents, true);
}


void Graph::appendSample(qreal value)
{
    m_samples << value;
    m_samplesChanged = true;
    update();
}


void Graph::removeFirstSample()
{
    m_samples.removeFirst();
    m_samplesChanged = true;
    update();
}

void Graph::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    m_geometryChanged = true;
    update();
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
}


class GraphNode : public QSGNode
{
public:
    NoisyNode *background;
    GridNode *grid;
    LineNode *line;
    LineNode *shadow;
};


QSGNode *Graph::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    GraphNode *n= static_cast<GraphNode *>(oldNode);

    QRectF rect = boundingRect();

    if (rect.isEmpty()) {
        delete n;
        return 0;
    }

    if (!n) {
        n = new GraphNode();

        n->background = new NoisyNode(window());
        n->grid = new GridNode();
        n->line = new LineNode(10, 0.5, QColor("steelblue"));
        n->shadow = new LineNode(20, 0.2, QColor::fromRgbF(0.2, 0.2, 0.2, 0.4));

        n->appendChildNode(n->background);
        n->appendChildNode(n->grid);
        n->appendChildNode(n->shadow);
        n->appendChildNode(n->line);
    }

    if (m_geometryChanged) {
        n->background->setRect(rect);
        n->grid->setRect(rect);
    }

    if (m_geometryChanged || m_samplesChanged) {
        n->line->updateGeometry(rect, m_samples);
        // We don't need to calculate the geometry twice, so just steal it from the other one...
        n->shadow->setGeometry(n->line->geometry());
    }

    m_geometryChanged = false;
    m_samplesChanged = false;

    return n;
}
