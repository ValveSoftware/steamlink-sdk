/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "avfvideowidget.h"
#include <QtCore/QDebug>

#include <AVFoundation/AVFoundation.h>
#include <QtGui/QResizeEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>

#if defined(Q_OS_MACOS)
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

QT_USE_NAMESPACE

AVFVideoWidget::AVFVideoWidget(QWidget *parent)
    : QWidget(parent)
    , m_aspectRatioMode(Qt::KeepAspectRatio)
    , m_playerLayer(0)
    , m_nativeView(0)
{
    setAutoFillBackground(false);
}

AVFVideoWidget::~AVFVideoWidget()
{
#ifdef QT_DEBUG_AVF
    qDebug() << Q_FUNC_INFO;
#endif

    if (m_playerLayer) {
        [m_playerLayer removeFromSuperlayer];
        [m_playerLayer release];
    }
}

QSize AVFVideoWidget::sizeHint() const
{
    return m_nativeSize;
}

Qt::AspectRatioMode AVFVideoWidget::aspectRatioMode() const
{
    return m_aspectRatioMode;
}

void AVFVideoWidget::setAspectRatioMode(Qt::AspectRatioMode mode)
{
    if (m_aspectRatioMode != mode) {
        m_aspectRatioMode = mode;

        updateAspectRatio();
    }
}

void AVFVideoWidget::setPlayerLayer(AVPlayerLayer *layer)
{
    if (m_playerLayer == layer)
        return;

    if (!m_nativeView) {
        //make video widget a native window
#if defined(Q_OS_OSX)
        m_nativeView = (NSView*)this->winId();
        [m_nativeView setWantsLayer:YES];
#else
        m_nativeView = (UIView*)this->winId();
#endif
    }

    if (m_playerLayer) {
        [m_playerLayer removeFromSuperlayer];
        [m_playerLayer release];
    }

    m_playerLayer = layer;

    CALayer *nativeLayer = [m_nativeView layer];

    if (layer) {
        [layer retain];

        m_nativeSize = QSize(m_playerLayer.bounds.size.width,
                             m_playerLayer.bounds.size.height);

        updateAspectRatio();
        [nativeLayer addSublayer:m_playerLayer];
        updatePlayerLayerBounds(this->size());
    }
#ifdef QT_DEBUG_AVF
    NSArray *sublayers = [nativeLayer sublayers];
    qDebug() << "playerlayer: " << "at z:" << [m_playerLayer zPosition]
                << " frame: " << m_playerLayer.frame.size.width << "x"  << m_playerLayer.frame.size.height;
    qDebug() << "superlayer: " << "at z:" << [nativeLayer zPosition]
                << " frame: " << nativeLayer.frame.size.width << "x"  << nativeLayer.frame.size.height;
    int i = 0;
    for (CALayer *layer in sublayers) {
        qDebug() << "layer " << i << ": at z:" << [layer zPosition]
                    << " frame: " << layer.frame.size.width << "x"  << layer.frame.size.height;
        i++;
    }
#endif

}

void AVFVideoWidget::resizeEvent(QResizeEvent *event)
{
    updatePlayerLayerBounds(event->size());
    QWidget::resizeEvent(event);
}

void AVFVideoWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    QWidget::paintEvent(event);
}

void AVFVideoWidget::updateAspectRatio()
{
    if (m_playerLayer) {
        switch (m_aspectRatioMode) {
        case Qt::IgnoreAspectRatio:
            [m_playerLayer setVideoGravity:AVLayerVideoGravityResize];
            break;
        case Qt::KeepAspectRatio:
            [m_playerLayer setVideoGravity:AVLayerVideoGravityResizeAspect];
            break;
        case Qt::KeepAspectRatioByExpanding:
            [m_playerLayer setVideoGravity:AVLayerVideoGravityResizeAspectFill];
            break;
        default:
            break;
        }
    }
}

void AVFVideoWidget::updatePlayerLayerBounds(const QSize &size)
{
    m_playerLayer.bounds = CGRectMake(0.0f, 0.0f, (float)size.width(), (float)size.height());
}
