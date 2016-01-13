/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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
