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

#ifndef MOCKVIDEODEVICESELECTORCONTROL_H
#define MOCKVIDEODEVICESELECTORCONTROL_H

#include <qvideodeviceselectorcontrol.h>

class MockVideoDeviceSelectorControl : public QVideoDeviceSelectorControl
{
    Q_OBJECT
public:
    MockVideoDeviceSelectorControl(QObject *parent)
        : QVideoDeviceSelectorControl(parent)
        , m_selectedDevice(1)
    {
    }

    ~MockVideoDeviceSelectorControl() { }

    int deviceCount() const { return availableCameras().count(); }

    QString deviceName(int index) const { return QString::fromLatin1(availableCameras().at(index)); }
    QString deviceDescription(int index) const { return cameraDescription(availableCameras().at(index)); }

    int defaultDevice() const { return availableCameras().indexOf(defaultCamera()); }
    int selectedDevice() const { return m_selectedDevice; }
    void setSelectedDevice(int index)
    {
        m_selectedDevice = index;
        emit selectedDeviceChanged(m_selectedDevice);
        emit selectedDeviceChanged(deviceName(m_selectedDevice));
    }

    static QByteArray defaultCamera()
    {
        return "othercamera";
    }

    static QList<QByteArray> availableCameras()
    {
        return QList<QByteArray>() << "backcamera" << "othercamera";
    }

    static QString cameraDescription(const QByteArray &camera)
    {
        if (camera == "backcamera")
            return QStringLiteral("backcamera desc");
        else if (camera == "othercamera")
            return QStringLiteral("othercamera desc");
        else
            return QString();
    }

private:
    int m_selectedDevice;
    QStringList m_devices;
    QStringList m_descriptions;
};

#endif // MOCKVIDEODEVICESELECTORCONTROL_H
