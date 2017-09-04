/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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

#include <QPainter>
#include <QApplication>

#include "embeddedsvgviewer.h"



EmbeddedSvgViewer::EmbeddedSvgViewer(const QString &filePath)
{
    qApp->setStyleSheet(" QSlider:vertical { width: 50px; } \
                          QSlider::groove:vertical { border: 1px solid black; border-radius: 3px; width: 6px; } \
                          QSlider::handle:vertical { height: 25px; margin: 0 -22px; image: url(':/files/v-slider-handle.svg'); } \
                       ");

    m_renderer = new QSvgRenderer(filePath);
    m_imageSize = m_renderer->viewBox().size();

    m_viewBoxCenter = (QPointF(m_imageSize.width() / qreal(2.0), m_imageSize.height() / qreal(2.0)));

    m_zoomSlider = new QSlider(Qt::Vertical, this);
    m_zoomSlider->setMaximum(150);
    m_zoomSlider->setMinimum(1);

    connect(m_zoomSlider, SIGNAL(valueChanged(int)), this, SLOT(setZoom(int)));
    m_zoomSlider->setValue(100);

    m_quitButton = new QPushButton("Quit", this);

    connect(m_quitButton, SIGNAL(pressed()), QApplication::instance(), SLOT(quit()));

    if (m_renderer->animated())
        connect(m_renderer, SIGNAL(repaintNeeded()), this, SLOT(update()));

}

void EmbeddedSvgViewer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    m_renderer->setViewBox(m_viewBox);
    m_renderer->render(&painter);
}


void EmbeddedSvgViewer::mouseMoveEvent ( QMouseEvent * event )
{
    int incX = int((event->globalX() - m_mousePress.x()) * m_imageScale);
    int incY = int((event->globalY() - m_mousePress.y()) * m_imageScale);

    QPointF newCenter;
    newCenter.setX(m_viewBoxCenterOnMousePress.x() - incX);
    newCenter.setY(m_viewBoxCenterOnMousePress.y() - incY);
  
    QRectF newViewBox = getViewBox(newCenter);


    // Do a bounded move on the horizontal:
    if ( (newViewBox.left() >= m_viewBoxBounds.left()) &&
         (newViewBox.right() <= m_viewBoxBounds.right()) ) 
    {
        m_viewBoxCenter.setX(newCenter.x());
        m_viewBox.setLeft(newViewBox.left());
        m_viewBox.setRight(newViewBox.right());
    }

    // do a bounded move on the vertical:
    if (  (newViewBox.top() >= m_viewBoxBounds.top()) &&
          (newViewBox.bottom() <= m_viewBoxBounds.bottom()) )
    {
        m_viewBoxCenter.setY(newCenter.y());
        m_viewBox.setTop(newViewBox.top());
        m_viewBox.setBottom(newViewBox.bottom());
    }

    update();
}

void EmbeddedSvgViewer::mousePressEvent ( QMouseEvent * event )
{
    m_viewBoxCenterOnMousePress = m_viewBoxCenter;
    m_mousePress = event->globalPos();
}


QRectF EmbeddedSvgViewer::getViewBox(QPointF viewBoxCenter)
{
    QRectF result;
    result.setLeft(viewBoxCenter.x() - (m_viewBoxSize.width() / 2));
    result.setTop(viewBoxCenter.y() - (m_viewBoxSize.height() / 2));
    result.setRight(viewBoxCenter.x() + (m_viewBoxSize.width() / 2));
    result.setBottom(viewBoxCenter.y() + (m_viewBoxSize.height() / 2));
    return result;
}

void EmbeddedSvgViewer::updateImageScale()
{
    m_imageScale = qMax( (qreal)m_imageSize.width() / (qreal)width(), 
                               (qreal)m_imageSize.height() / (qreal)height())*m_zoomLevel;

    m_viewBoxSize.setWidth((qreal)width() * m_imageScale);
    m_viewBoxSize.setHeight((qreal)height() * m_imageScale);
}


void EmbeddedSvgViewer::resizeEvent ( QResizeEvent * /* event */ )
{
    qreal origZoom = m_zoomLevel;

    // Get the new bounds:
    m_zoomLevel = 1.0;
    updateImageScale();
    m_viewBoxBounds = getViewBox(QPointF(m_imageSize.width() / 2.0, m_imageSize.height() / 2.0));

    m_zoomLevel = origZoom;
    updateImageScale();
    m_viewBox = getViewBox(m_viewBoxCenter);

    QRect sliderRect;
    sliderRect.setLeft(width() - m_zoomSlider->sizeHint().width());
    sliderRect.setRight(width());
    sliderRect.setTop(height()/4);
    sliderRect.setBottom(height() - (height()/4));
    m_zoomSlider->setGeometry(sliderRect);
}


void EmbeddedSvgViewer::setZoom(int newZoom)
{
    m_zoomLevel = qreal(newZoom) / qreal(100);
    
    updateImageScale();
    m_viewBox = getViewBox(m_viewBoxCenter);

    update();
}





