/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickdialring_p.h"

#include <QtCore/qmath.h>
#include <QtGui/qpainter.h>
#include <QtQuick/private/qquickitem_p.h>

QT_BEGIN_NAMESPACE

QQuickDialRing::QQuickDialRing(QQuickItem *parent) :
    QQuickPaintedItem(parent),
    m_progress(0),
    m_color(Qt::black)
{
}

qreal QQuickDialRing::progress() const
{
    return m_progress;
}

void QQuickDialRing::setProgress(qreal progress)
{
    if (progress == m_progress)
        return;

    m_progress = progress;
    update();
    emit progressChanged();
}

QColor QQuickDialRing::color() const
{
    return m_color;
}

void QQuickDialRing::setColor(const QColor &color)
{
    if (color == m_color)
        return;

    m_color = color;
    update();
    emit colorChanged();
}

void QQuickDialRing::paint(QPainter *painter)
{
    if (width() <= 0 || height() <= 0)
        return;

    QPen pen(m_color);
    pen.setWidth(8);
    pen.setCapStyle(Qt::FlatCap);
    painter->setPen(pen);

    const QRectF bounds = boundingRect();
    const qreal smallest = qMin(bounds.width(), bounds.height());
    QRectF rect = QRectF(pen.widthF() / 2.0 + 1, pen.widthF() / 2.0 + 1, smallest - pen.widthF() - 2, smallest - pen.widthF() - 2);
    rect.moveCenter(bounds.center());

    // Make sure the arc is aligned to whole pixels.
    if (rect.x() - int(rect.x()) > 0)
        rect.setX(qCeil(rect.x()));
    if (rect.y() - int(rect.y()) > 0)
        rect.setY(qCeil(rect.y()));
    if (rect.width() - int(rect.width()) > 0)
        rect.setWidth(qFloor(rect.width()));
    if (rect.height() - int(rect.height()) > 0)
        rect.setHeight(qFloor(rect.height()));

    painter->setRenderHint(QPainter::Antialiasing);

    const qreal startAngle = (140 + 90);
    const qreal spanAngle = (m_progress * 280) * -1;
    QPainterPath path;
    path.arcMoveTo(rect, startAngle);
    path.arcTo(rect, startAngle, spanAngle);
    painter->drawPath(path);

    rect.adjust(-pen.widthF() / 2.0, -pen.widthF() / 2.0, pen.widthF() / 2.0, pen.widthF() / 2.0);
    pen.setWidth(1);
    painter->setPen(pen);

    path = QPainterPath();
    path.arcMoveTo(rect, 0);
    path.arcTo(rect, 0, 360);
    painter->drawPath(path);
}

QT_END_NAMESPACE
