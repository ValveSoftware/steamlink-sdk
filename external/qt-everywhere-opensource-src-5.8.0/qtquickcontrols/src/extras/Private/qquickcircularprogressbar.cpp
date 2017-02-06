/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Extras module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickcircularprogressbar_p.h"

#include <QtGui/QPen>
#include <QtGui/QPainter>
#include <QtMath>

QQuickCircularProgressBar::QQuickCircularProgressBar(QQuickItem *parent) :
    QQuickPaintedItem(parent),
    mProgress(0),
    mBarWidth(1),
    mInset(0),
    mBackgroundColor(QColor(Qt::transparent)),
    mMinimumValueAngle(-90 * 16),
    mMaximumValueAngle(-450 * 16)
{
    setFlag(ItemHasContents, true);
}

QQuickCircularProgressBar::~QQuickCircularProgressBar()
{
}

void QQuickCircularProgressBar::paint(QPainter *painter)
{
    QPen pen(Qt::red);
    pen.setWidthF(mBarWidth);

    const QRectF bounds = boundingRect();
    const qreal smallest = qMin(bounds.width(), bounds.height());
    QRectF rect = QRectF(pen.widthF() / 2.0, pen.widthF() / 2.0, smallest - pen.widthF(), smallest - pen.widthF());
    rect.adjust(mInset, mInset, -mInset, -mInset);

    // Make sure the arc is aligned to whole pixels.
    if (rect.x() - int(rect.x()) > 0) {
        rect.setX(qCeil(rect.x()));
    }
    if (rect.y() - int(rect.y()) > 0) {
        rect.setY(qCeil(rect.y()));
    }
    if (rect.width() - int(rect.width()) > 0) {
        rect.setWidth(qFloor(rect.width()));
    }
    if (rect.height() - int(rect.height()) > 0) {
        rect.setHeight(qFloor(rect.height()));
    }

    painter->setRenderHint(QPainter::Antialiasing);

    // QPainter::drawArc uses positive values for counter clockwise - the opposite of our API -
    // so we must reverse the angles with * -1. Also, our angle origin is at 12 o'clock, whereas
    // QPainter's is 3 o'clock, hence - 90.
    const qreal startAngle = ((mMinimumValueAngle - 90) * -1) * 16;
    if (mBackgroundColor != Qt::transparent) {
        QBrush bgBrush(mBackgroundColor);
        QPen bgPen;
        bgPen.setWidthF(mBarWidth);
        bgPen.setBrush(bgBrush);
        painter->setPen(bgPen);
        painter->drawArc(rect, startAngle, (((mMaximumValueAngle - 90) * -1 * 16) - startAngle));
    }

    QLinearGradient gradient;
    gradient.setStart(bounds.width() / 2, mInset);
    gradient.setFinalStop(bounds.width() / 2, bounds.height() - mInset);
    gradient.setStops(mGradientStops);

    QBrush brush(gradient);
    pen.setBrush(brush);

    painter->setPen(pen);

    const qreal spanAngle = progress() * (((mMaximumValueAngle - 90) * -1 * 16) - startAngle);
    painter->drawArc(rect, startAngle, spanAngle);
}

qreal QQuickCircularProgressBar::progress() const
{
    return mProgress;
}

void QQuickCircularProgressBar::setProgress(qreal progress)
{
    if (mProgress != progress) {
        mProgress = progress;
        emit progressChanged(mProgress);
        update();
    }
}

qreal QQuickCircularProgressBar::barWidth() const
{
    return mBarWidth;
}

void QQuickCircularProgressBar::setBarWidth(qreal barWidth)
{
    if (mBarWidth != barWidth) {
        mBarWidth = barWidth;
        emit barWidthChanged(mBarWidth);
        update();
    }
}

qreal QQuickCircularProgressBar::inset() const
{
    return mInset;
}

void QQuickCircularProgressBar::setInset(qreal inset)
{
    if (mInset != inset) {
        mInset = inset;
        emit insetChanged(mInset);
        update();
    }
}

qreal QQuickCircularProgressBar::minimumValueAngle() const
{
    return mMinimumValueAngle;
}

void QQuickCircularProgressBar::setMinimumValueAngle(qreal minimumValueAngle)
{
    if (mMinimumValueAngle != minimumValueAngle) {
        mMinimumValueAngle = minimumValueAngle;
        emit minimumValueAngleChanged(mMinimumValueAngle);
        update();
    }
}

qreal QQuickCircularProgressBar::maximumValueAngle() const
{
    return mMinimumValueAngle;
}

void QQuickCircularProgressBar::setMaximumValueAngle(qreal maximumValueAngle)
{
    if (mMaximumValueAngle != maximumValueAngle) {
        mMaximumValueAngle = maximumValueAngle;
        emit maximumValueAngleChanged(mMaximumValueAngle);
        update();
    }
}

void QQuickCircularProgressBar::clearStops()
{
    mGradientStops.clear();
}

void QQuickCircularProgressBar::addStop(qreal position, const QColor &color)
{
    mGradientStops.append(QGradientStop(position, color));
}

void QQuickCircularProgressBar::redraw()
{
    update();
}

QColor QQuickCircularProgressBar::backgroundColor() const
{
    return mBackgroundColor;
}

void QQuickCircularProgressBar::setBackgroundColor(const QColor &backgroundColor)
{
    if (mBackgroundColor != backgroundColor) {
        mBackgroundColor = backgroundColor;
        emit backgroundColorChanged(backgroundColor);
        update();
    }
}
