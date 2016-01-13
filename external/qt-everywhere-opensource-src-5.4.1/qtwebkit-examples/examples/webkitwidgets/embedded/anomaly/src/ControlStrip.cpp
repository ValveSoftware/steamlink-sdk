/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
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

#include "ControlStrip.h"

#include <QtCore>
#include <QtWidgets>

ControlStrip::ControlStrip(QWidget *parent)
    : QWidget(parent)
{
    menuPixmap.load(":/images/edit-find.png");
    backPixmap.load(":/images/go-previous.png");
    forwardPixmap.load(":/images/go-next.png");
    closePixmap.load(":/images/button-close.png");
}

QSize ControlStrip::sizeHint() const
{
    return minimumSizeHint();
}

QSize ControlStrip::minimumSizeHint() const
{
    return QSize(320, 48);
}

void ControlStrip::mousePressEvent(QMouseEvent *event)
{
    int h = height();
    int spacing = qMin(h, (width() - h * 4) / 3);
    int x = event->pos().x();

    if (x < h) {
        emit menuClicked();
        event->accept();
        return;
    }

    if (x > width() - h) {
        emit closeClicked();
        event->accept();
        return;
    }

    if ((x < width() - (h + spacing)) && (x > width() - (h * 2 + spacing))) {
        emit forwardClicked();
        event->accept();
        return;
    }

    if ((x < width() - (h * 2 + spacing * 2)) && (x > width() - (h * 3 + spacing * 2))) {
        emit backClicked();
        event->accept();
        return;
    }
}

void ControlStrip::paintEvent(QPaintEvent *event)
{
    int h = height();
    int spacing = qMin(h, (width() - h * 4) / 3);
    int s = (height() - menuPixmap.height()) / 2;

    QPainter p(this);
    p.fillRect(event->rect(), QColor(32, 32, 32, 192));
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.drawPixmap(s, s, menuPixmap);
    p.drawPixmap(width() - h + s, s, closePixmap);
    p.drawPixmap(width() - (h * 2 + spacing) + s, s, forwardPixmap);
    p.drawPixmap(width() - (h * 3 + spacing * 2) + s, s, backPixmap);

    p.end();
}
