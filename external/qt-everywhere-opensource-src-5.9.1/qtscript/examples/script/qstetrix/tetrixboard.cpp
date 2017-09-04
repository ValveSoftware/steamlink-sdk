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

#include "tetrixboard.h"

#include <QtWidgets>

Q_DECLARE_METATYPE(QPainter*)

TetrixBoard::TetrixBoard(QWidget *parent)
    : QFrame(parent)
{
    timer = new QTimer(this);
    qMetaTypeId<QPainter*>();
}

void TetrixBoard::setNextPieceLabel(QWidget *label)
{
    nextPieceLbl = qobject_cast<QLabel*>(label);
}

QLabel *TetrixBoard::nextPieceLabel() const
{ 
    return nextPieceLbl;
}

QObject *TetrixBoard::getTimer()
{
    return timer;
}

QSize TetrixBoard::minimumSizeHint() const
{
    return QSize(BoardWidth * 5 + frameWidth() * 2,
                 BoardHeight * 5 + frameWidth() * 2);
}

void TetrixBoard::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);
    QPainter painter(this);
    painter.drawImage(0, 0, image);
}

void TetrixBoard::keyPressEvent(QKeyEvent *event)
{
    emit keyPressed(event->key());
}

void TetrixBoard::showNextPiece(int width, int height)
{
    if (!nextPieceLabel())
        return;

    QPixmap pixmap(width * squareWidth(), height * squareHeight());
    QPainter painter(&pixmap);
    painter.fillRect(pixmap.rect(), nextPieceLabel()->palette().background());

    emit paintNextPieceRequested(&painter);

    nextPieceLabel()->setPixmap(pixmap);
}

void TetrixBoard::drawPauseScreen(QPainter *painter)
{
    painter->drawText(contentsRect(), Qt::AlignCenter, tr("Pause"));
}

void TetrixBoard::drawSquare(QPainter *painter, int x, int y, int shape)
{
    static const QRgb colorTable[8] = {
        0x000000, 0xCC6666, 0x66CC66, 0x6666CC,
        0xCCCC66, 0xCC66CC, 0x66CCCC, 0xDAAA00
    };

    x = x*squareWidth();
    y = y*squareHeight();

    QColor color = colorTable[shape];
    painter->fillRect(x + 1, y + 1, squareWidth() - 2, squareHeight() - 2,
                      color);

    painter->setPen(color.light());
    painter->drawLine(x, y + squareHeight() - 1, x, y);
    painter->drawLine(x, y, x + squareWidth() - 1, y);

    painter->setPen(color.dark());
    painter->drawLine(x + 1, y + squareHeight() - 1,
                      x + squareWidth() - 1, y + squareHeight() - 1);
    painter->drawLine(x + squareWidth() - 1, y + squareHeight() - 1,
                      x + squareWidth() - 1, y + 1);
}

void TetrixBoard::update()
{
    QRect rect = contentsRect();
    if (image.size() != rect.size())
        image = QImage(rect.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(qRgba(0,0,0,0));
    QPainter painter;
    painter.begin(&image);
    int boardTop = rect.bottom() - BoardHeight*squareHeight();
    painter.translate(rect.left(), boardTop);
    emit paintRequested(&painter);
    QFrame::update();
}
