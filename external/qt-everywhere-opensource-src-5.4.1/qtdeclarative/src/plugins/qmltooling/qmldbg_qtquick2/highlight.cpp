/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "highlight.h"

#include <QtCore/QTimer>
#include <QtGui/QPainter>
#include <QtGui/QStaticText>
#include <QtQuick/QQuickWindow>

namespace QmlJSDebugger {
namespace QtQuick2 {

Highlight::Highlight(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    initRenderDetails();
}

Highlight::Highlight(QQuickItem *item, QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    initRenderDetails();
    setItem(item);
}

void Highlight::initRenderDetails()
{
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setPerformanceHint(QQuickPaintedItem::FastFBOResizing, true);
}

void Highlight::setItem(QQuickItem *item)
{
    if (m_item)
        m_item->disconnect(this);

    if (item) {
        connect(item, SIGNAL(xChanged()), SLOT(adjust()));
        connect(item, SIGNAL(yChanged()), SLOT(adjust()));
        connect(item, SIGNAL(widthChanged()), SLOT(adjust()));
        connect(item, SIGNAL(heightChanged()), SLOT(adjust()));
        connect(item, SIGNAL(rotationChanged()), SLOT(adjust()));
        connect(item, SIGNAL(transformOriginChanged(TransformOrigin)),
                SLOT(adjust()));
    }
    QQuickWindow *view = item->window();
    QQuickItem * contentItem = view->contentItem();
    if (contentItem) {
        connect(contentItem, SIGNAL(xChanged()), SLOT(adjust()));
        connect(contentItem, SIGNAL(yChanged()), SLOT(adjust()));
        connect(contentItem, SIGNAL(widthChanged()), SLOT(adjust()));
        connect(contentItem, SIGNAL(heightChanged()), SLOT(adjust()));
        connect(contentItem, SIGNAL(rotationChanged()), SLOT(adjust()));
        connect(contentItem, SIGNAL(transformOriginChanged(TransformOrigin)),
                SLOT(adjust()));
    }
    m_item = item;
    setContentsSize(view->size());
    adjust();
}

void Highlight::adjust()
{
    if (!m_item)
        return;

    bool success = false;
    m_transform = m_item->itemTransform(0, &success);
    if (!success)
        m_transform = QTransform();

    setSize(QSizeF(m_item->width(), m_item->height()));
    qreal scaleFactor = 1;
    QPointF originOffset = QPointF(0,0);
    QQuickWindow *view = m_item->window();
    if (view->contentItem()) {
        scaleFactor = view->contentItem()->scale();
        originOffset -= view->contentItem()->position();
    }
    // The scale transform for the overlay needs to be cancelled
    // as the Item's transform which will be applied to the painter
    // takes care of it.
    parentItem()->setScale(1/scaleFactor);
    setPosition(originOffset);
    update();
}


void HoverHighlight::paint(QPainter *painter)
{
    if (!item())
        return;

    painter->save();
    painter->setTransform(transform());
    painter->setPen(QColor(108, 141, 221));
    painter->drawRect(QRect(0, 0, item()->width() - 1, item()->height() - 1));
    painter->restore();
}


SelectionHighlight::SelectionHighlight(const QString &name, QQuickItem *item, QQuickItem *parent)
    : Highlight(item, parent),
      m_name(name),
      m_nameDisplayActive(false)
{
}

void SelectionHighlight::paint(QPainter *painter)
{
    if (!item())
        return;
    painter->save();
    painter->fillRect(QRectF(0,0,contentsSize().width(), contentsSize().height()),
                      QColor(0,0,0,127));
    painter->setTransform(transform());
    // Setting the composition mode such that the transparency will
    // be erased as per the selected item.
    painter->setCompositionMode(QPainter::CompositionMode_Clear);
    painter->fillRect(0, 0, item()->width(), item()->height(), Qt::black);
    painter->restore();

    // Use the painter with the original transform and not with the
    // item's transform for display of name.
    if (!m_nameDisplayActive)
        return;

    // Paint the text in gray background if display name is active..
    QRect textRect = painter->boundingRect(QRect(10, contentsSize().height() - 10 ,
                                 contentsSize().width() - 20, contentsSize().height()),
                                 Qt::AlignCenter | Qt::ElideRight, m_name);

    qreal xPosition = m_displayPoint.x();
    if (xPosition + textRect.width() > contentsSize().width())
        xPosition = contentsSize().width() - textRect.width();
    if (xPosition < 0) {
        xPosition = 0;
        textRect.setWidth(contentsSize().width());
    }
    qreal yPosition = m_displayPoint.y() - textRect.height() - 20;
    if (yPosition < 50 )
        yPosition = 50;

    painter->fillRect(QRectF(xPosition - 5, yPosition - 5,
                      textRect.width() + 10, textRect.height() + 10), Qt::gray);
    painter->drawRect(QRectF(xPosition - 5, yPosition - 5,
                      textRect.width() + 10, textRect.height() + 10));

    painter->drawStaticText(xPosition, yPosition, QStaticText(m_name));
}

void SelectionHighlight::showName(const QPointF &displayPoint)
{
    m_displayPoint = displayPoint;
    m_nameDisplayActive = true;
    QTimer::singleShot(1500, this, SLOT(disableNameDisplay()));
    update();
}

void SelectionHighlight::disableNameDisplay()
{
    m_nameDisplayActive = false;
    update();
}

} // namespace QtQuick2
} // namespace QmlJSDebugger
