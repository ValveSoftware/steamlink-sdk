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

#include "qquickflatprogressbar_p.h"

#include <QtCore/QtMath>
#include <QtGui/QPainter>

QQuickFlatProgressBar::QQuickFlatProgressBar(QQuickItem *parent) :
    QQuickPaintedItem(parent),
    mStripeOffset(0),
    mRadius(0),
    mIndeterminate(false)
{
    mAnimation.setTargetObject(this);
    mAnimation.setPropertyName("stripeOffset");
    mAnimation.setEndValue(0);
    mAnimation.setDuration(800);
    mAnimation.setLoopCount(-1);

    connect(this, SIGNAL(stripeOffsetChanged(qreal)), this, SLOT(repaint()));
    connect(this, SIGNAL(progressChanged(qreal)), this, SLOT(repaint()));
    connect(this, SIGNAL(enabledChanged()), this, SLOT(repaint()));
    connect(this, SIGNAL(indeterminateChanged(bool)), this, SLOT(repaint()));
    connect(this, SIGNAL(widthChanged()), this, SLOT(onWidthChanged()));
    connect(this, SIGNAL(heightChanged()), this, SLOT(onHeightChanged()));
    connect(this, SIGNAL(visibleChanged()), this, SLOT(onVisibleChanged()));
}

int QQuickFlatProgressBar::barWidth()
{
    // 14 is the design height of stripes on the bar, 10 is design width of stripe
    return int(height() * 10 / 14);
}

void QQuickFlatProgressBar::paint(QPainter *painter)
{
    // This item should always be rounded to an integer size, so it is safe to use int here.
    const int w = width();
    const int h = height();

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::transparent);

    // Draw the background.
    painter->setBrush(isIndeterminate() && mProgress > 0 ? QColor(92, 170, 21) : QColor(0, 0, 0, 255 * 0.15));
    painter->drawRoundedRect(0, 0, w, h, mRadius, mRadius);

    // Draw progress, or indeterminate stripes.
    painter->setClipPath(mClipPath);

    if (!mIndeterminate) {
        painter->setBrush(isEnabled() ? QColor(92, 170, 21) : QColor(179, 179, 179));
        painter->drawRect(0, 0, int(w * mProgress), h);
    } else {
        QPainterPath innerClipPath;
        // 1 is the design margin thickness.
        const int margin = qMax(1, int(height() * (1.0 / 16.0)));
        // We take the margin from the radius so that the inner and outer radii match each other visually.
        innerClipPath.addRoundedRect(margin, margin, width() - margin * 2, height() - margin * 2, mRadius - margin, mRadius - margin);
        painter->setClipPath(innerClipPath);

        painter->translate(mStripeOffset, 0);

        const qreal stripeWidth = barWidth();
        // The horizontal distance created by the slant of the stripe.
        const qreal stripeSlantDistance = h;

        // One stripe width is equal to the height of the bar.
        QPainterPath stripe;
        stripe.moveTo(0, h);
        stripe.lineTo(stripeSlantDistance, 0);
        stripe.lineTo(stripeSlantDistance + stripeWidth, 0);
        stripe.lineTo(stripeWidth, h);
        stripe.closeSubpath();

        painter->setBrush(QBrush(mProgress > 0 ? QColor(255, 255, 255, 77) : Qt::white));

        for (int i = -stripeWidth * 2; i < w + stripeWidth * 2; i += stripeWidth * 2) {
            painter->translate(i, 0);
            painter->drawPath(stripe);
            painter->translate(-i, 0);
        }

        painter->translate(-mStripeOffset, 0);
    }
}

qreal QQuickFlatProgressBar::stripeOffset() const
{
    return mStripeOffset;
}

void QQuickFlatProgressBar::setStripeOffset(qreal pos)
{
    if (pos != mStripeOffset) {
        mStripeOffset = pos;
        emit stripeOffsetChanged(pos);
    }
}

qreal QQuickFlatProgressBar::progress() const
{
    return mProgress;
}

void QQuickFlatProgressBar::setProgress(qreal progress)
{
    if (progress != mProgress) {
        mProgress = progress;
        emit progressChanged(mProgress);
    }
}

bool QQuickFlatProgressBar::isIndeterminate()
{
    return mIndeterminate;
}

void QQuickFlatProgressBar::setIndeterminate(bool indeterminate)
{
    if (indeterminate != mIndeterminate) {
        mIndeterminate = indeterminate;
        emit indeterminateChanged(mIndeterminate);
    }
}

void QQuickFlatProgressBar::repaint()
{
    QQuickPaintedItem::update(QRect(0, 0, width(), height()));
}

void QQuickFlatProgressBar::restartAnimation()
{
    mAnimation.stop();
    mAnimation.setStartValue(-barWidth() * 2);
    mAnimation.start();
}

void QQuickFlatProgressBar::onVisibleChanged()
{
    if (isVisible()) {
        restartAnimation();
    } else {
        mAnimation.stop();
    }
}

void QQuickFlatProgressBar::onWidthChanged()
{
    restartAnimation();

    mClipPath = QPainterPath();
    mClipPath.addRoundedRect(0, 0, width(), height(), mRadius, mRadius);
}

void QQuickFlatProgressBar::onHeightChanged()
{
    restartAnimation();

    // 3 is the design radius, 16 is the design height of the bar
    const qreal radius = height() * 3 / 16;
    if (radius != mRadius) {
        mRadius = radius;
    }

    mClipPath = QPainterPath();
    mClipPath.addRoundedRect(0, 0, width(), height(), mRadius, mRadius);
}
