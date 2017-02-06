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

#include "qquicktexthandle.h"

QQuickTextHandle::QQuickTextHandle(QQuickItem *parent) :
    QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

QQuickTextHandle::~QQuickTextHandle()
{
}

void QQuickTextHandle::paint(QPainter *painter)
{
    painter->save();
    paintBulb(painter, QColor(0, 0, 0, 38), true);
    painter->restore();

    paintBulb(painter, mColor, false);
    painter->fillRect((mType == SelectionHandle ? 10 : 1), 0, 2, 23, mColor);
}

QQuickTextHandle::TextHandleType QQuickTextHandle::type() const
{
    return mType;
}

void QQuickTextHandle::setType(QQuickTextHandle::TextHandleType type)
{
    if (mType != type) {
        mType = type;
        update();
        emit typeChanged();
    }
}

QColor QQuickTextHandle::color() const
{
    return mColor;
}

void QQuickTextHandle::setColor(const QColor &color)
{
    if (mColor != color) {
        mColor = color;
        update();
        emit colorChanged();
    }
}

qreal QQuickTextHandle::scaleFactor() const
{
    return mScaleFactor;
}

void QQuickTextHandle::setScaleFactor(qreal scaleFactor)
{
    if (mScaleFactor != scaleFactor) {
        mScaleFactor = scaleFactor;
        setImplicitWidth(qRound(28 * mScaleFactor));
        // + 2 for shadows
        setImplicitHeight(qRound((32 + 2) * mScaleFactor));
        update();
        emit scaleFactorChanged();
    }
}

void QQuickTextHandle::paintBulb(QPainter *painter, const QColor &color, bool isShadow)
{
    painter->scale(mScaleFactor, mScaleFactor);
    QPainterPath path;

    if (mType == SelectionHandle) {
        painter->translate(16, isShadow ? 2 : 0);
        path.moveTo(10.242, 28.457);
        path.cubicTo(7.8980000000000015, 30.799, 4.099000000000001, 30.799,1.7560000000000002, 28.457);
        path.cubicTo(-0.5859999999999999, 26.115000000000002, -0.5859999999999999,22.314, 1.7560000000000002, 19.973);
        path.cubicTo(4.748, 16.980999999999998, 11.869, 18.343999999999998, 11.869, 18.343999999999998);
        path.cubicTo(11.869, 18.343999999999998, 13.244, 25.455, 10.242, 28.457);

    } else {
        if (isShadow)
            painter->translate(0, 2);
        path.moveTo(2.757,28.457);
        path.cubicTo(5.101,30.799,8.899000000000001,30.799,11.243,28.457);
        path.cubicTo(13.585,26.115000000000002,13.585,22.314,11.243,19.973);
        path.cubicTo(8.251,16.98,1.13,18.344,1.13,18.344);
        path.cubicTo(1.13,18.344,-0.245,25.455,2.757,28.457);
    }

    painter->fillPath(path, QBrush(color));
}

