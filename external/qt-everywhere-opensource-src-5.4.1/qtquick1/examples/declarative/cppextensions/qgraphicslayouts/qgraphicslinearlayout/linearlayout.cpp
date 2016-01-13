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

#include "linearlayout.h"

#include <QGraphicsWidget>
#include <QGraphicsLayoutItem>

LinearLayoutAttached::LinearLayoutAttached(QObject *parent)
: QObject(parent), m_stretch(1), m_alignment(Qt::AlignTop), m_spacing(0)
{
}

void LinearLayoutAttached::setStretchFactor(int f)
{
    if (m_stretch != f) {
        m_stretch = f;
        emit stretchChanged(reinterpret_cast<QGraphicsLayoutItem*>(parent()), m_stretch);
    }
}

void LinearLayoutAttached::setSpacing(int s)
{
    if (m_spacing != s) {
        m_spacing = s;
        emit spacingChanged(reinterpret_cast<QGraphicsLayoutItem*>(parent()), m_spacing);
    }
}

void LinearLayoutAttached::setAlignment(Qt::Alignment a)
{
    if (m_alignment != a) {
        m_alignment = a;
        emit alignmentChanged(reinterpret_cast<QGraphicsLayoutItem*>(parent()), m_alignment);
    }
}

GraphicsLinearLayoutStretchItemObject::GraphicsLinearLayoutStretchItemObject(QObject *parent)
    : QObject(parent)
{
}

QSizeF GraphicsLinearLayoutStretchItemObject::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    Q_UNUSED(which);
    Q_UNUSED(constraint);
    return QSizeF();
}


GraphicsLinearLayoutObject::GraphicsLinearLayoutObject(QObject *parent)
: QObject(parent)
{
}

GraphicsLinearLayoutObject::~GraphicsLinearLayoutObject()
{
}

void GraphicsLinearLayoutObject::insertLayoutItem(int index, QGraphicsLayoutItem *item)
{
    insertItem(index, item);

    if (LinearLayoutAttached *obj = attachedProperties.value(item)) {
        setStretchFactor(item, obj->stretchFactor());
        setAlignment(item, obj->alignment());
        updateSpacing(item, obj->spacing());
        QObject::connect(obj, SIGNAL(stretchChanged(QGraphicsLayoutItem*,int)),
                         this, SLOT(updateStretch(QGraphicsLayoutItem*,int)));
        QObject::connect(obj, SIGNAL(alignmentChanged(QGraphicsLayoutItem*,Qt::Alignment)),
                         this, SLOT(updateAlignment(QGraphicsLayoutItem*,Qt::Alignment)));
        QObject::connect(obj, SIGNAL(spacingChanged(QGraphicsLayoutItem*,int)),
                         this, SLOT(updateSpacing(QGraphicsLayoutItem*,int)));
    }
}

void GraphicsLinearLayoutObject::removeAt(int index)
{
    QGraphicsLayoutItem *item = itemAt(index);
    if (item) {
        LinearLayoutAttached *obj = attachedProperties.value(item);
        obj->disconnect(this);
        attachedProperties.remove(item);
    }
    QGraphicsLinearLayout::removeAt(index);
}

void GraphicsLinearLayoutObject::clearChildren()
{
    // do not delete the removed items; they will be deleted by the QML engine
    while (count() > 0)
        removeAt(count()-1);
}

qreal GraphicsLinearLayoutObject::contentsMargin() const
{
    qreal a, b, c, d;
    getContentsMargins(&a, &b, &c, &d);
    if (a == b && a == c && a == d)
        return a;
    return -1;
}

void GraphicsLinearLayoutObject::setContentsMargin(qreal m)
{
    setContentsMargins(m, m, m, m);
}

void GraphicsLinearLayoutObject::updateStretch(QGraphicsLayoutItem *item, int stretch)
{
    QGraphicsLinearLayout::setStretchFactor(item, stretch);
}

void GraphicsLinearLayoutObject::updateSpacing(QGraphicsLayoutItem* item, int spacing)
{
    for (int i=0; i < count(); i++){
        if (itemAt(i) == item) {
            QGraphicsLinearLayout::setItemSpacing(i, spacing);
            return;
        }
    }
}

void GraphicsLinearLayoutObject::updateAlignment(QGraphicsLayoutItem *item, Qt::Alignment alignment)
{
    QGraphicsLinearLayout::setAlignment(item, alignment);
}

QHash<QGraphicsLayoutItem*, LinearLayoutAttached*> GraphicsLinearLayoutObject::attachedProperties;
LinearLayoutAttached *GraphicsLinearLayoutObject::qmlAttachedProperties(QObject *obj)
{
    LinearLayoutAttached *rv = new LinearLayoutAttached(obj);
    if (qobject_cast<QGraphicsLayoutItem*>(obj))
        attachedProperties.insert(qobject_cast<QGraphicsLayoutItem*>(obj), rv);
    return rv;
}


