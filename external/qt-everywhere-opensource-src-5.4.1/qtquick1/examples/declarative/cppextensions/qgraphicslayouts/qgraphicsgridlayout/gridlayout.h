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

#ifndef GRIDLAYOUT_H
#define GRIDLAYOUT_H

#include <qdeclarative.h>

#include <QGraphicsGridLayout>
#include <QGraphicsLayoutItem>

class GridLayoutAttached;
class GraphicsGridLayoutObject : public QObject, public QGraphicsGridLayout
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsLayout QGraphicsLayoutItem)

    Q_PROPERTY(QDeclarativeListProperty<QGraphicsLayoutItem> children READ children)
    Q_PROPERTY(qreal spacing READ spacing WRITE setSpacing)
    Q_PROPERTY(qreal contentsMargin READ contentsMargin WRITE setContentsMargin)
    Q_PROPERTY(qreal verticalSpacing READ verticalSpacing WRITE setVerticalSpacing)
    Q_PROPERTY(qreal horizontalSpacing READ horizontalSpacing WRITE setHorizontalSpacing)
    Q_CLASSINFO("DefaultProperty", "children")

public:
    GraphicsGridLayoutObject(QObject * = 0);
    ~GraphicsGridLayoutObject();

    QDeclarativeListProperty<QGraphicsLayoutItem> children() { return QDeclarativeListProperty<QGraphicsLayoutItem>(this, 0, children_append, children_count, children_at, children_clear); }

    qreal spacing() const;
    qreal contentsMargin() const;
    void setContentsMargin(qreal);

    void removeAt(int index);

    static GridLayoutAttached *qmlAttachedProperties(QObject *);

private slots:
    void updateAlignment(QGraphicsLayoutItem*,Qt::Alignment);

private:
    friend class GraphicsLayoutAttached;

    void addWidget(QGraphicsWidget *);
    void clearChildren();
    void addLayoutItem(QGraphicsLayoutItem *);

    static void children_append(QDeclarativeListProperty<QGraphicsLayoutItem> *prop, QGraphicsLayoutItem *item) {
        static_cast<GraphicsGridLayoutObject*>(prop->object)->addLayoutItem(item);
    }

    static void children_clear(QDeclarativeListProperty<QGraphicsLayoutItem> *prop) {
        static_cast<GraphicsGridLayoutObject*>(prop->object)->clearChildren();
    }

    static int children_count(QDeclarativeListProperty<QGraphicsLayoutItem> *prop) {
        return static_cast<GraphicsGridLayoutObject*>(prop->object)->count();
    }

    static QGraphicsLayoutItem *children_at(QDeclarativeListProperty<QGraphicsLayoutItem> *prop, int index) {
        return static_cast<GraphicsGridLayoutObject*>(prop->object)->itemAt(index);
    }

    static QHash<QGraphicsLayoutItem*, GridLayoutAttached*> attachedProperties;
};


class GridLayoutAttached : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int row READ row WRITE setRow)
    Q_PROPERTY(int column READ column WRITE setColumn)

    Q_PROPERTY(int rowSpan READ rowSpan WRITE setRowSpan)
    Q_PROPERTY(int columnSpan READ columnSpan WRITE setColumnSpan)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)

    Q_PROPERTY(int rowStretchFactor READ rowStretchFactor WRITE setRowStretchFactor)
    Q_PROPERTY(int columnStretchFactor READ columnStretchFactor WRITE setColumnStretchFactor)
    Q_PROPERTY(int rowSpacing READ rowSpacing WRITE setRowSpacing)
    Q_PROPERTY(int columnSpacing READ columnSpacing WRITE setColumnSpacing)

    Q_PROPERTY(int rowPreferredHeight READ rowPreferredHeight WRITE setRowPreferredHeight)
    Q_PROPERTY(int rowMinimumHeight READ rowMinimumHeight WRITE setRowMinimumHeight)
    Q_PROPERTY(int rowMaximumHeight READ rowMaximumHeight WRITE setRowMaximumHeight)
    Q_PROPERTY(int rowFixedHeight READ rowFixedHeight WRITE setRowFixedHeight)

    Q_PROPERTY(int columnPreferredWidth READ columnPreferredWidth WRITE setColumnPreferredWidth)
    Q_PROPERTY(int columnMaximumWidth READ columnMaximumWidth WRITE setColumnMaximumWidth)
    Q_PROPERTY(int columnMinimumWidth READ columnMinimumWidth WRITE setColumnMinimumWidth)
    Q_PROPERTY(int columnFixedWidth READ columnFixedWidth WRITE setColumnFixedWidth)

public:
    GridLayoutAttached(QObject *parent);

    int row() const { return m_row; }
    void setRow(int r) { m_row = r; }

    int column() const { return m_column; }
    void setColumn(int c) { m_column = c; }

    int rowSpan() const { return m_rowspan; }
    void setRowSpan(int rs) { m_rowspan = rs; }

    int columnSpan() const { return m_colspan; }
    void setColumnSpan(int cs) { m_colspan = cs; }

    Qt::Alignment alignment() const { return m_alignment; }
    void setAlignment(Qt::Alignment a);

    int rowStretchFactor() const { return m_rowStretch; }
    void setRowStretchFactor(int f) { m_rowStretch = f; }
    int columnStretchFactor() const { return m_colStretch; }
    void setColumnStretchFactor(int f) { m_colStretch = f; }

    int rowSpacing() const { return m_rowSpacing; }
    void setRowSpacing(int s) { m_rowSpacing = s; }
    int columnSpacing() const { return m_colSpacing; }
    void setColumnSpacing(int s) { m_colSpacing = s; }

    int rowPreferredHeight() const { return m_rowPrefHeight; }
    void setRowPreferredHeight(int s) { m_rowPrefHeight = s; }

    int rowMaximumHeight() const { return m_rowMaxHeight; }
    void setRowMaximumHeight(int s) { m_rowMaxHeight = s; }

    int rowMinimumHeight() const { return m_rowMinHeight; }
    void setRowMinimumHeight(int s) { m_rowMinHeight = s; }

    int rowFixedHeight() const { return m_rowFixHeight; }
    void setRowFixedHeight(int s) { m_rowFixHeight = s; }

    int columnPreferredWidth() const { return m_colPrefwidth; }
    void setColumnPreferredWidth(int s) { m_colPrefwidth = s; }

    int columnMaximumWidth() const { return m_colMaxwidth; }
    void setColumnMaximumWidth(int s) { m_colMaxwidth = s; }

    int columnMinimumWidth() const { return m_colMinwidth; }
    void setColumnMinimumWidth(int s) { m_colMinwidth = s; }

    int columnFixedWidth() const { return m_colFixwidth; }
    void setColumnFixedWidth(int s) { m_colFixwidth = s; }

signals:
    void alignmentChanged(QGraphicsLayoutItem*, Qt::Alignment);

private:
    int m_row;
    int m_column;

    int m_rowspan;
    int m_colspan;
    Qt::Alignment m_alignment;

    int m_rowStretch;
    int m_colStretch;
    int m_rowSpacing;
    int m_colSpacing;

    int m_rowPrefHeight;
    int m_rowMaxHeight;
    int m_rowMinHeight;
    int m_rowFixHeight;

    int m_colPrefwidth;
    int m_colMaxwidth;
    int m_colMinwidth;
    int m_colFixwidth;
};

QML_DECLARE_INTERFACE(QGraphicsLayoutItem)
QML_DECLARE_INTERFACE(QGraphicsLayout)
QML_DECLARE_TYPE(GraphicsGridLayoutObject)
QML_DECLARE_TYPEINFO(GraphicsGridLayoutObject, QML_HAS_ATTACHED_PROPERTIES)

#endif

