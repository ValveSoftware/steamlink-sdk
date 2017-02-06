/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef AVFVIDEOWINDOWCONTROL_H
#define AVFVIDEOWINDOWCONTROL_H

#include <QVideoWindowControl>

@class AVPlayerLayer;
#if defined(Q_OS_OSX)
@class NSView;
typedef NSView NativeView;
#else
@class UIView;
typedef UIView NativeView;
#endif

#include "avfvideooutput.h"

QT_BEGIN_NAMESPACE

class AVFVideoWindowControl : public QVideoWindowControl, public AVFVideoOutput
{
    Q_OBJECT
    Q_INTERFACES(AVFVideoOutput)

public:
    AVFVideoWindowControl(QObject *parent = 0);
    virtual ~AVFVideoWindowControl();

    // QVideoWindowControl interface
public:
    WId winId() const;
    void setWinId(WId id);

    QRect displayRect() const;
    void setDisplayRect(const QRect &rect);

    bool isFullScreen() const;
    void setFullScreen(bool fullScreen);

    void repaint();
    QSize nativeSize() const;

    Qt::AspectRatioMode aspectRatioMode() const;
    void setAspectRatioMode(Qt::AspectRatioMode mode);

    int brightness() const;
    void setBrightness(int brightness);

    int contrast() const;
    void setContrast(int contrast);

    int hue() const;
    void setHue(int hue);

    int saturation() const;
    void setSaturation(int saturation);

    // AVFVideoOutput interface
    void setLayer(void *playerLayer);

private:
    void updateAspectRatio();
    void updatePlayerLayerBounds();

    WId m_winId;
    QRect m_displayRect;
    bool m_fullscreen;
    int m_brightness;
    int m_contrast;
    int m_hue;
    int m_saturation;
    Qt::AspectRatioMode m_aspectRatioMode;
    QSize m_nativeSize;
    AVPlayerLayer *m_playerLayer;
    NativeView *m_nativeView;
};

QT_END_NAMESPACE

#endif // AVFVIDEOWINDOWCONTROL_H
