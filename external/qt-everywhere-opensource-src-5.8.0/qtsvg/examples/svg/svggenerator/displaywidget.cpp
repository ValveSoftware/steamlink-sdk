/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
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
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
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

#include <QtWidgets>
#include "displaywidget.h"

DisplayWidget::DisplayWidget(QWidget *parent)
    : QWidget(parent)
{
    QPainterPath car;
    QPainterPath house;

    QFile file(":resources/shapes.dat");
    file.open(QFile::ReadOnly);
    QDataStream stream(&file);
    stream >> car >> house >> tree >> moon;
    file.close();

    shapeMap[Car] = car;
    shapeMap[House] = house;

    background = Sky;
    shapeColor = Qt::darkYellow;
    shape = House;
}

//! [paint event]
void DisplayWidget::paintEvent(QPaintEvent * /* event */)
{
    QPainter painter;
    painter.begin(this);
    painter.setRenderHint(QPainter::Antialiasing);
    paint(painter);
    painter.end();
}
//! [paint event]

//! [paint function]
void DisplayWidget::paint(QPainter &painter)
{
//![paint picture]
    painter.setClipRect(QRect(0, 0, 200, 200));
    painter.setPen(Qt::NoPen);

    switch (background) {
    case Sky:
    default:
        painter.fillRect(QRect(0, 0, 200, 200), Qt::darkBlue);
        painter.translate(145, 10);
        painter.setBrush(Qt::white);
        painter.drawPath(moon);
        painter.translate(-145, -10);
        break;
    case Trees:
    {
        painter.fillRect(QRect(0, 0, 200, 200), Qt::darkGreen);
        painter.setBrush(Qt::green);
        painter.setPen(Qt::black);
        for (int y = -55, row = 0; y < 200; y += 50, ++row) {
            int xs;
            if (row == 2 || row == 3)
                xs = 150;
            else
                xs = 50;
            for (int x = 0; x < 200; x += xs) {
                painter.save();
                painter.translate(x, y);
                painter.drawPath(tree);
                painter.restore();
            }
        }
        break;
    }
    case Road:
        painter.fillRect(QRect(0, 0, 200, 200), Qt::gray);
        painter.setPen(QPen(Qt::white, 4, Qt::DashLine));
        painter.drawLine(QLine(0, 35, 200, 35));
        painter.drawLine(QLine(0, 165, 200, 165));
        break;
    }

    painter.setBrush(shapeColor);
    painter.setPen(Qt::black);
    painter.translate(100, 100);
    painter.drawPath(shapeMap[shape]);
//![paint picture]
}
//! [paint function]

QColor DisplayWidget::color() const
{
    return shapeColor;
}

void DisplayWidget::setBackground(Background background)
{
    this->background = background;
    update();
}

void DisplayWidget::setColor(const QColor &color)
{
    this->shapeColor = color;
    update();
}

void DisplayWidget::setShape(Shape shape)
{
    this->shape = shape;
    update();
}
