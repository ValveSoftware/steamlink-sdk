/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <private/bar_p.h>
#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsSceneEvent>
#include <QtWidgets/QStyleOptionGraphicsItem>
#include <QtWidgets/QStyle>

QT_CHARTS_BEGIN_NAMESPACE

Bar::Bar(QBarSet *barset, QGraphicsItem *parent) : QGraphicsRectItem(parent),
    m_index(-255),
    m_layoutIndex(-255),
    m_barset(barset),
    m_labelItem(nullptr),
    m_hovering(false),
    m_mousePressed(false),
    m_visualsDirty(true),
    m_labelDirty(true)
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable);
}

Bar::~Bar()
{
    // End hover event, if bar is deleted during it
    if (m_hovering)
        emit hovered(false, m_index, m_barset);
    delete m_labelItem;
}

void Bar::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    emit pressed(m_index, m_barset);
    m_mousePressed = true;
    QGraphicsItem::mousePressEvent(event);
}

void Bar::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    m_hovering = true;
    emit hovered(true, m_index, m_barset);

}

void Bar::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    m_hovering = false;
    emit hovered(false, m_index, m_barset);
}

void Bar::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    emit released(m_index, m_barset);
    if (m_mousePressed)
        emit clicked(m_index, m_barset);
    m_mousePressed = false;
    QGraphicsItem::mouseReleaseEvent(event);
}

void Bar::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    emit doubleClicked(m_index, m_barset);
    QGraphicsItem::mouseDoubleClickEvent(event);
}

void Bar::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    // Remove selection border around bar
    QStyleOptionGraphicsItem barOption(*option);
    barOption.state &= ~QStyle::State_Selected;
    QGraphicsRectItem::paint(painter, &barOption, widget);
}

#include "moc_bar_p.cpp"

QT_CHARTS_END_NAMESPACE
