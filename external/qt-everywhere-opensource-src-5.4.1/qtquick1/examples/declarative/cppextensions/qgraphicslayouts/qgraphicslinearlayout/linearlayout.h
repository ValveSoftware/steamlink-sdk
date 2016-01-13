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

#ifndef LINEARLAYOUT_H
#define LINEARLAYOUT_H

#include <qdeclarative.h>

#include <QGraphicsLinearLayout>
#include <QGraphicsLayoutItem>

class GraphicsLinearLayoutStretchItemObject : public QObject, public QGraphicsLayoutItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsLayoutItem)
public:
    GraphicsLinearLayoutStretchItemObject(QObject *parent = 0);

    virtual QSizeF sizeHint(Qt::SizeHint, const QSizeF &) const;
};


class LinearLayoutAttached;
class GraphicsLinearLayoutObject : public QObject, public QGraphicsLinearLayout
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsLayout QGraphicsLayoutItem)

    Q_PROPERTY(QDeclarativeListProperty<QGraphicsLayoutItem> children READ children)
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation)
    Q_PROPERTY(qreal spacing READ spacing WRITE setSpacing)
    Q_PROPERTY(qreal contentsMargin READ contentsMargin WRITE setContentsMargin)
    Q_CLASSINFO("DefaultProperty", "children")
public:
    GraphicsLinearLayoutObject(QObject * = 0);
    ~GraphicsLinearLayoutObject();

    QDeclarativeListProperty<QGraphicsLayoutItem> children() { return QDeclarativeListProperty<QGraphicsLayoutItem>(this, 0, children_append, children_count, children_at, children_clear); }

    qreal contentsMargin() const;
    void setContentsMargin(qreal);

    void removeAt(int index);

    static LinearLayoutAttached *qmlAttachedProperties(QObject *);

private slots:
    void updateStretch(QGraphicsLayoutItem*,int);
    void updateSpacing(QGraphicsLayoutItem*,int);
    void updateAlignment(QGraphicsLayoutItem*,Qt::Alignment);

private:
    friend class LinearLayoutAttached;

    void clearChildren();
    void insertLayoutItem(int, QGraphicsLayoutItem *);

    static void children_append(QDeclarativeListProperty<QGraphicsLayoutItem> *prop, QGraphicsLayoutItem *item) {
        static_cast<GraphicsLinearLayoutObject*>(prop->object)->insertLayoutItem(-1, item);
    }

    static void children_clear(QDeclarativeListProperty<QGraphicsLayoutItem> *prop) {
        static_cast<GraphicsLinearLayoutObject*>(prop->object)->clearChildren();
    }

    static int children_count(QDeclarativeListProperty<QGraphicsLayoutItem> *prop) {
        return static_cast<GraphicsLinearLayoutObject*>(prop->object)->count();
    }

    static QGraphicsLayoutItem *children_at(QDeclarativeListProperty<QGraphicsLayoutItem> *prop, int index) {
        return static_cast<GraphicsLinearLayoutObject*>(prop->object)->itemAt(index);
    }

    static QHash<QGraphicsLayoutItem*, LinearLayoutAttached*> attachedProperties;
};


class LinearLayoutAttached : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int stretchFactor READ stretchFactor WRITE setStretchFactor NOTIFY stretchChanged)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment NOTIFY alignmentChanged)
    Q_PROPERTY(int spacing READ spacing WRITE setSpacing NOTIFY spacingChanged)

public:
    LinearLayoutAttached(QObject *parent);

    int stretchFactor() const { return m_stretch; }
    void setStretchFactor(int f);
    Qt::Alignment alignment() const { return m_alignment; }
    void setAlignment(Qt::Alignment a);
    int spacing() const { return m_spacing; }
    void setSpacing(int s);

signals:
    void stretchChanged(QGraphicsLayoutItem*, int);
    void alignmentChanged(QGraphicsLayoutItem*, Qt::Alignment);
    void spacingChanged(QGraphicsLayoutItem*, int);

private:
    int m_stretch;
    Qt::Alignment m_alignment;
    int m_spacing;
};

QML_DECLARE_INTERFACE(QGraphicsLayoutItem)
QML_DECLARE_INTERFACE(QGraphicsLayout)
QML_DECLARE_TYPE(GraphicsLinearLayoutStretchItemObject)
QML_DECLARE_TYPE(GraphicsLinearLayoutObject)
QML_DECLARE_TYPEINFO(GraphicsLinearLayoutObject, QML_HAS_ATTACHED_PROPERTIES)

#endif


