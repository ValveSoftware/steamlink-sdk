/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "avfvideowidgetcontrol.h"
#include "avfvideowidget.h"

#ifdef QT_DEBUG_AVF
#include <QtCore/QDebug>
#endif

#import <AVFoundation/AVFoundation.h>

QT_USE_NAMESPACE

AVFVideoWidgetControl::AVFVideoWidgetControl(QObject *parent)
    : QVideoWidgetControl(parent)
    , m_fullscreen(false)
    , m_brightness(0)
    , m_contrast(0)
    , m_hue(0)
    , m_saturation(0)
{
    m_videoWidget = new AVFVideoWidget(0);
}

AVFVideoWidgetControl::~AVFVideoWidgetControl()
{
#ifdef QT_DEBUG_AVF
    qDebug() << Q_FUNC_INFO;
#endif
    delete m_videoWidget;
}

void AVFVideoWidgetControl::setLayer(void *playerLayer)
{
#ifdef QT_DEBUG_AVF
    qDebug() << Q_FUNC_INFO << playerLayer;
#endif

    m_videoWidget->setPlayerLayer((AVPlayerLayer*)playerLayer);

}

QWidget *AVFVideoWidgetControl::videoWidget()
{
    return m_videoWidget;
}

bool AVFVideoWidgetControl::isFullScreen() const
{
    return m_fullscreen;
}

void AVFVideoWidgetControl::setFullScreen(bool fullScreen)
{
    m_fullscreen = fullScreen;
}

Qt::AspectRatioMode AVFVideoWidgetControl::aspectRatioMode() const
{
    return m_videoWidget->aspectRatioMode();
}

void AVFVideoWidgetControl::setAspectRatioMode(Qt::AspectRatioMode mode)
{
    m_videoWidget->setAspectRatioMode(mode);
}

int AVFVideoWidgetControl::brightness() const
{
    return m_brightness;
}

void AVFVideoWidgetControl::setBrightness(int brightness)
{
    m_brightness = brightness;
}

int AVFVideoWidgetControl::contrast() const
{
    return m_contrast;
}

void AVFVideoWidgetControl::setContrast(int contrast)
{
    m_contrast = contrast;
}

int AVFVideoWidgetControl::hue() const
{
    return m_hue;
}

void AVFVideoWidgetControl::setHue(int hue)
{
    m_hue = hue;
}

int AVFVideoWidgetControl::saturation() const
{
    return m_saturation;
}

void AVFVideoWidgetControl::setSaturation(int saturation)
{
    m_saturation = saturation;
}

#include "moc_avfvideowidgetcontrol.cpp"
