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

#include "avfvideowindowcontrol.h"

#include <AVFoundation/AVFoundation.h>

QT_USE_NAMESPACE

AVFVideoWindowControl::AVFVideoWindowControl(QObject *parent)
    : QVideoWindowControl(parent)
    , m_winId(0)
    , m_fullscreen(false)
    , m_brightness(0)
    , m_contrast(0)
    , m_hue(0)
    , m_saturation(0)
    , m_aspectRatioMode(Qt::IgnoreAspectRatio)
    , m_playerLayer(0)
    , m_nativeView(0)
{
}

AVFVideoWindowControl::~AVFVideoWindowControl()
{
    if (m_playerLayer) {
        [m_playerLayer removeFromSuperlayer];
        [m_playerLayer release];
    }
}

WId AVFVideoWindowControl::winId() const
{
    return m_winId;
}

void AVFVideoWindowControl::setWinId(WId id)
{
    m_winId = id;
    m_nativeView = (NativeView*)m_winId;
}

QRect AVFVideoWindowControl::displayRect() const
{
    return m_displayRect;
}

void AVFVideoWindowControl::setDisplayRect(const QRect &rect)
{
    if (m_displayRect != rect) {
        m_displayRect = rect;
        updatePlayerLayerBounds();
    }
}

bool AVFVideoWindowControl::isFullScreen() const
{
    return m_fullscreen;
}

void AVFVideoWindowControl::setFullScreen(bool fullScreen)
{
    if (m_fullscreen != fullScreen) {
        m_fullscreen = fullScreen;
        Q_EMIT QVideoWindowControl::fullScreenChanged(fullScreen);
    }
}

void AVFVideoWindowControl::repaint()
{
    if (m_playerLayer)
        [m_playerLayer setNeedsDisplay];
}

QSize AVFVideoWindowControl::nativeSize() const
{
    return m_nativeSize;
}

Qt::AspectRatioMode AVFVideoWindowControl::aspectRatioMode() const
{
    return m_aspectRatioMode;
}

void AVFVideoWindowControl::setAspectRatioMode(Qt::AspectRatioMode mode)
{
    if (m_aspectRatioMode != mode) {
        m_aspectRatioMode = mode;
        updateAspectRatio();
    }
}

int AVFVideoWindowControl::brightness() const
{
    return m_brightness;
}

void AVFVideoWindowControl::setBrightness(int brightness)
{
    if (m_brightness != brightness) {
        m_brightness = brightness;
        Q_EMIT QVideoWindowControl::brightnessChanged(brightness);
    }
}

int AVFVideoWindowControl::contrast() const
{
    return m_contrast;
}

void AVFVideoWindowControl::setContrast(int contrast)
{
    if (m_contrast != contrast) {
        m_contrast = contrast;
        Q_EMIT QVideoWindowControl::contrastChanged(contrast);
    }
}

int AVFVideoWindowControl::hue() const
{
    return m_hue;
}

void AVFVideoWindowControl::setHue(int hue)
{
    if (m_hue != hue) {
        m_hue = hue;
        Q_EMIT QVideoWindowControl::hueChanged(hue);
    }
}

int AVFVideoWindowControl::saturation() const
{
    return m_saturation;
}

void AVFVideoWindowControl::setSaturation(int saturation)
{
    if (m_saturation != saturation) {
        m_saturation = saturation;
        Q_EMIT QVideoWindowControl::saturationChanged(saturation);
    }
}

void AVFVideoWindowControl::setLayer(void *playerLayer)
{
    AVPlayerLayer *layer = (AVPlayerLayer*)playerLayer;
    if (m_playerLayer == layer)
        return;

    if (!m_winId) {
        qDebug("AVFVideoWindowControl: No video window");
        return;
    }

#if defined(Q_OS_OSX)
    [m_nativeView setWantsLayer:YES];
#endif

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
        updatePlayerLayerBounds();
    }
}

void AVFVideoWindowControl::updateAspectRatio()
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

void AVFVideoWindowControl::updatePlayerLayerBounds()
{
    if (m_playerLayer) {
        CGRect newBounds = CGRectMake(0, 0,
                                      m_displayRect.width(), m_displayRect.height());
        m_playerLayer.bounds = newBounds;
        m_playerLayer.position = CGPointMake(m_displayRect.x(), m_displayRect.y());
    }
}

#include "moc_avfvideowindowcontrol.cpp"
