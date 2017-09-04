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
#include <QtSvg>
#include "mimedata.h"
#include "sourcewidget.h"

SourceWidget::SourceWidget(QWidget *parent)
    : QWidget(parent)
{
    QFile imageFile(":/images/example.svg");
    imageFile.open(QIODevice::ReadOnly);
    imageData = imageFile.readAll();
    imageFile.close();

    QScrollArea *imageArea = new QScrollArea;
    imageLabel = new QSvgWidget;
    imageLabel->renderer()->load(imageData);
    imageArea->setWidget(imageLabel);
    //imageLabel->setMinimumSize(imageLabel->renderer()->viewBox().size());

    QLabel *instructTopLabel = new QLabel(tr("This is an SVG drawing:"));
    QLabel *instructBottomLabel = new QLabel(
        tr("Drag the icon to copy the drawing as a PNG file:"));
    instructBottomLabel->setWordWrap(true);
    QPushButton *dragIcon = new QPushButton(tr("Export"));
    dragIcon->setIcon(QIcon(":/images/drag.png"));

    connect(dragIcon, SIGNAL(pressed()), this, SLOT(startDrag()));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(instructTopLabel, 0, 0, 1, 2);
    layout->addWidget(imageArea, 1, 0, 2, 2);
    layout->addWidget(instructBottomLabel, 3, 0);
    layout->addWidget(dragIcon, 3, 1);
    setLayout(layout);
    setWindowTitle(tr("Delayed Encoding"));
}

//![1]
void SourceWidget::createData(const QString &mimeType)
{
    if (mimeType != "image/png")
        return;

    QImage image(imageLabel->size(), QImage::Format_RGB32);
    QPainter painter;
    painter.begin(&image);
    imageLabel->renderer()->render(&painter);
    painter.end();

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    buffer.close();

    mimeData->setData("image/png", data);
}
//![1]

//![0]
void SourceWidget::startDrag()
{
    mimeData = new MimeData;

    connect(mimeData, SIGNAL(dataRequested(QString)),
            this, SLOT(createData(QString)), Qt::DirectConnection);

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(QPixmap(":/images/drag.png"));

    drag->exec(Qt::CopyAction);
}
//![0]

