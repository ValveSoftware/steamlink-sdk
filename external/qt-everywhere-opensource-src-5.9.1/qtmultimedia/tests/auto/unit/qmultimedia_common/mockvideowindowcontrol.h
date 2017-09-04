/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef MOCKVIDEOWINDOWCONTROL_H
#define MOCKVIDEOWINDOWCONTROL_H

#include "qvideowindowcontrol.h"

class MockVideoWindowControl : public QVideoWindowControl
{
public:
    MockVideoWindowControl(QObject *parent = 0) : QVideoWindowControl(parent) {}
    WId winId() const { return 0; }
    void setWinId(WId) {}
    QRect displayRect() const { return QRect(); }
    void setDisplayRect(const QRect &) {}
    bool isFullScreen() const { return false; }
    void setFullScreen(bool) {}
    void repaint() {}
    QSize nativeSize() const { return QSize(); }
    Qt::AspectRatioMode aspectRatioMode() const { return Qt::KeepAspectRatio; }
    void setAspectRatioMode(Qt::AspectRatioMode) {}
    int brightness() const { return 0; }
    void setBrightness(int) {}
    int contrast() const { return 0; }
    void setContrast(int) {}
    int hue() const { return 0; }
    void setHue(int) {}
    int saturation() const { return 0; }
    void setSaturation(int) {}
};

#endif // MOCKVIDEOWINDOWCONTROL_H
