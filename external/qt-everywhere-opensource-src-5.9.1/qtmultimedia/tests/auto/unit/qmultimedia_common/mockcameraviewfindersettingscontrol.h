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

#ifndef MOCKCAMERAVIEWFINDERSETTINGSCONTROL_H
#define MOCKCAMERAVIEWFINDERSETTINGSCONTROL_H

#include "qcameraviewfindersettingscontrol.h"

class MockCameraViewfinderSettingsControl : public QCameraViewfinderSettingsControl2
{
    Q_OBJECT
public:
    MockCameraViewfinderSettingsControl(QObject *parent = 0):
            QCameraViewfinderSettingsControl2(parent)
    {
        QCameraViewfinderSettings s;
        s.setResolution(640, 480);
        s.setMinimumFrameRate(30);
        s.setMaximumFrameRate(30);
        s.setPixelFormat(QVideoFrame::Format_NV12);
        s.setPixelAspectRatio(1, 1);
        supportedSettings.append(s);

        s.setResolution(1280, 720);
        s.setMinimumFrameRate(10);
        s.setMaximumFrameRate(10);
        s.setPixelFormat(QVideoFrame::Format_NV12);
        s.setPixelAspectRatio(1, 1);
        supportedSettings.append(s);

        s.setResolution(1920, 1080);
        s.setMinimumFrameRate(5);
        s.setMaximumFrameRate(10);
        s.setPixelFormat(QVideoFrame::Format_BGR32);
        s.setPixelAspectRatio(2, 1);
        supportedSettings.append(s);

        s.setResolution(1280, 720);
        s.setMinimumFrameRate(10);
        s.setMaximumFrameRate(10);
        s.setPixelFormat(QVideoFrame::Format_YV12);
        s.setPixelAspectRatio(1, 1);
        supportedSettings.append(s);

        s.setResolution(1280, 720);
        s.setMinimumFrameRate(30);
        s.setMaximumFrameRate(30);
        s.setPixelFormat(QVideoFrame::Format_YV12);
        s.setPixelAspectRatio(1, 1);
        supportedSettings.append(s);

        s.setResolution(320, 240);
        s.setMinimumFrameRate(30);
        s.setMaximumFrameRate(30);
        s.setPixelFormat(QVideoFrame::Format_NV12);
        s.setPixelAspectRatio(1, 1);
        supportedSettings.append(s);
    }

    ~MockCameraViewfinderSettingsControl() {}

    QCameraViewfinderSettings viewfinderSettings() const
    {
        return settings;
    }

    void setViewfinderSettings(const QCameraViewfinderSettings &s)
    {
        settings = s;
    }

    QList<QCameraViewfinderSettings> supportedViewfinderSettings() const
    {
        return supportedSettings;
    }

    QCameraViewfinderSettings settings;
    QList<QCameraViewfinderSettings> supportedSettings;
};

#endif // MOCKCAMERAVIEWFINDERSETTINGSCONTROL_H
