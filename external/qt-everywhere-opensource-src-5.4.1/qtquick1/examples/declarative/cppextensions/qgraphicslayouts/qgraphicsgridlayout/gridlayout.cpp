/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

#include "gridlayout.h"

#include <QGraphicsWidget>
#include <QDebug>

GridLayoutAttached::GridLayoutAttached(QObject *parent)
: QObject(parent), m_row(-1), m_column(-1), m_rowspan(1), m_colspan(1), m_alignment(-1), m_rowStretch(-1),
    m_colStretch(-1), m_rowSpacing(-1), m_colSpacing(-1), m_rowPrefHeight(-1), m_rowMaxHeight(-1), m_rowMinHeight(-1),
    m_rowFixHeight(-1), m_colPrefwidth(-1), m_colMaxwidth(-1), m_colMinwidth(-1), m_colFixwidth(-1)
{
}

void GridLayoutAttached::setAlignment(Qt::Alignment a)
{
    if (m_alignment != a) {
        m_alignment = a;
        emit alignmentChanged(reinterpret_cast<QGraphicsLayoutItem*>(parent()), m_alignment);
    }
}

QHash<QGraphicsLayoutItem*, GridLayoutAttached*> GraphicsGridLayoutObject::attachedProperties;

GraphicsGridLayoutObject::GraphicsGridLayoutObject(QObject *parent)
: QObject(parent)
{
}

GraphicsGridLayoutObject::~GraphicsGridLayoutObject()
{
}

void GraphicsGridLayoutObject::addWidget(QGraphicsWidget *widget)
{
    //use attached properties
    if (QObject *obj = attachedProperties.value(qobject_cast<QGraphicsLayoutItem*>(widget))) {
        int row = static_cast<GridLayoutAttached *>(obj)->row();
        int column = static_cast<GridLayoutAttached *>(obj)->column();
        int rowSpan = static_cast<GridLayoutAttached *>(obj)->rowSpan();
        int columnSpan = static_cast<GridLayoutAttached *>(obj)->columnSpan();
        if (row == -1 || column == -1) {
            qWarning() << "Must set row and column for an item in a grid layout";
            return;
        }
        addItem(widget, row, column, rowSpan, columnSpan);
    }
}

void GraphicsGridLayoutObject::addLayoutItem(QGraphicsLayoutItem *item)
{
    //use attached properties
    if (GridLayoutAttached *obj = attachedProperties.value(item)) {
        int row = obj->row();
        int column = obj->column();
        int rowSpan = obj->rowSpan();
        int columnSpan = obj->columnSpan();
        Qt::Alignment alignment = obj->alignment();

        if (row == -1 || column == -1) {
            qWarning() << "Must set row and column for an item in a grid layout";
            return;
        }

        if (obj->rowSpacing() != -1)
            setRowSpacing(row, obj->rowSpacing());
        if (obj->columnSpacing() != -1)
            setColumnSpacing(column, obj->columnSpacing());
        if (obj->rowStretchFactor() != -1)
            setRowStretchFactor(row, obj->rowStretchFactor());
        if (obj->columnStretchFactor() != -1)
            setColumnStretchFactor(column, obj->columnStretchFactor());
        if (obj->rowPreferredHeight() != -1)
            setRowPreferredHeight(row, obj->rowPreferredHeight());
        if (obj->rowMaximumHeight() != -1)
            setRowMaximumHeight(row, obj->rowMaximumHeight());
        if (obj->rowMinimumHeight() != -1)
            setRowMinimumHeight(row, obj->rowMinimumHeight());
        if (obj->rowFixedHeight() != -1)
            setRowFixedHeight(row, obj->rowFixedHeight());
        if (obj->columnPreferredWidth() != -1)
            setColumnPreferredWidth(row, obj->columnPreferredWidth());
        if (obj->columnMaximumWidth() != -1)
            setColumnMaximumWidth(row, obj->columnMaximumWidth());
        if (obj->columnMinimumWidth() != -1)
            setColumnMinimumWidth(row, obj->columnMinimumWidth());
        if (obj->columnFixedWidth() != -1)
            setColumnFixedWidth(row, obj->columnFixedWidth());

        addItem(item, row, column, rowSpan, columnSpan);

        if (alignment != -1)
            setAlignment(item, alignment);
        QObject::connect(obj, SIGNAL(alignmentChanged(QGraphicsLayoutItem*, Qt::Alignment)),
                         this, SLOT(updateAlignment(QGraphicsLayoutItem*, Qt::Alignment)));
    }
}

void GraphicsGridLayoutObject::removeAt(int index)
{
    QGraphicsLayoutItem *item = itemAt(index);
    if (item) {
        GridLayoutAttached *obj = attachedProperties.value(item);
        obj->disconnect(this);
        attachedProperties.remove(item);
    }
    QGraphicsGridLayout::removeAt(index);
}

void GraphicsGridLayoutObject::clearChildren()
{
    // do not delete the removed items; they will be deleted by the QML engine
    while (count() > 0)
        removeAt(count()-1);
}

qreal GraphicsGridLayoutObject::spacing() const
{
    if (verticalSpacing() == horizontalSpacing())
        return verticalSpacing();
    return -1;
}

qreal GraphicsGridLayoutObject::contentsMargin() const
{
    qreal a, b, c, d;
    getContentsMargins(&a, &b, &c, &d);
    if (a == b && a == c && a == d)
        return a;
    return -1;
}

void GraphicsGridLayoutObject::setContentsMargin(qreal m)
{
    setContentsMargins(m, m, m, m);
}

void GraphicsGridLayoutObject::updateAlignment(QGraphicsLayoutItem *item, Qt::Alignment alignment)
{
    QGraphicsGridLayout::setAlignment(item, alignment);
}

GridLayoutAttached *GraphicsGridLayoutObject::qmlAttachedProperties(QObject *obj)
{
    GridLayoutAttached *rv = new GridLayoutAttached(obj);
    if (qobject_cast<QGraphicsLayoutItem*>(obj))
        attachedProperties.insert(qobject_cast<QGraphicsLayoutItem*>(obj), rv);
    return rv;
}

