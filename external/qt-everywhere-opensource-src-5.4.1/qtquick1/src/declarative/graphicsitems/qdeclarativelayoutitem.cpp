/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "private/qdeclarativelayoutitem_p.h"

#include <QDebug>

#include <limits.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype LayoutItem
    \instantiates QDeclarativeLayoutItem
    \ingroup qml-utility-elements
    \since 4.7
    \brief The LayoutItem element allows declarative UI elements to be placed inside Qt's Graphics View layouts.

    LayoutItem is a variant of \l Item with additional size hint properties. These properties provide the size hints
    necessary for items to work in conjunction with Qt \l{Graphics View Framework}{Graphics View} layout classes
    such as QGraphicsLinearLayout and QGraphicsGridLayout. The Qt layout mechanisms will resize the LayoutItem as appropriate,
    taking its size hints into account, and you can propagate this to the other elements in your UI via anchors and bindings.

    This is a QGraphicsLayoutItem subclass, and its properties merely expose the QGraphicsLayoutItem functionality to QML.

    The \l{declarative/cppextensions/qgraphicslayouts/layoutitem}{LayoutItem example}
    demonstrates how a LayoutItem can be used within a \l{Graphics View Framework}{Graphics View}
    scene.
*/

/*!
    \qmlproperty QSizeF LayoutItem::maximumSize

    The maximumSize property can be set to specify the maximum desired size of this LayoutItem
*/

/*!
    \qmlproperty QSizeF LayoutItem::minimumSize

    The minimumSize property can be set to specify the minimum desired size of this LayoutItem
*/

/*!
    \qmlproperty QSizeF LayoutItem::preferredSize

    The preferredSize property can be set to specify the preferred size of this LayoutItem
*/

QDeclarativeLayoutItem::QDeclarativeLayoutItem(QDeclarativeItem* parent)
    : QDeclarativeItem(parent), m_maximumSize(INT_MAX,INT_MAX), m_minimumSize(0,0), m_preferredSize(0,0)
{
    setGraphicsItem(this);
}

void QDeclarativeLayoutItem::setGeometry(const QRectF & rect)
{
    setX(rect.x());
    setY(rect.y());
    setWidth(rect.width());
    setHeight(rect.height());
}

QSizeF QDeclarativeLayoutItem::sizeHint(Qt::SizeHint w, const QSizeF &constraint) const
{
    Q_UNUSED(constraint);
    if(w == Qt::MinimumSize){
        return m_minimumSize;
    }else if(w == Qt::MaximumSize){
        return m_maximumSize;
    }else{
        return m_preferredSize;
    }
}

QT_END_NAMESPACE
