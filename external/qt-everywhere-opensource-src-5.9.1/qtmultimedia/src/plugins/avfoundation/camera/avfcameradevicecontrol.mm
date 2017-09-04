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

#include "avfcameradebug.h"
#include "avfcameradevicecontrol.h"
#include "avfcameraservice.h"
#include "avfcamerasession.h"

QT_USE_NAMESPACE

AVFCameraDeviceControl::AVFCameraDeviceControl(AVFCameraService *service, QObject *parent)
   : QVideoDeviceSelectorControl(parent)
   , m_service(service)
   , m_selectedDevice(0)
   , m_dirty(true)
{
    Q_UNUSED(m_service);
}

AVFCameraDeviceControl::~AVFCameraDeviceControl()
{
}

int AVFCameraDeviceControl::deviceCount() const
{
    return AVFCameraSession::availableCameraDevices().count();
}

QString AVFCameraDeviceControl::deviceName(int index) const
{
    const QList<AVFCameraInfo> &devices = AVFCameraSession::availableCameraDevices();
    if (index < 0 || index >= devices.count())
        return QString();

    return QString::fromUtf8(devices.at(index).deviceId);
}

QString AVFCameraDeviceControl::deviceDescription(int index) const
{
    const QList<AVFCameraInfo> &devices = AVFCameraSession::availableCameraDevices();
    if (index < 0 || index >= devices.count())
        return QString();

    return devices.at(index).description;
}

int AVFCameraDeviceControl::defaultDevice() const
{
    return AVFCameraSession::defaultCameraIndex();
}

int AVFCameraDeviceControl::selectedDevice() const
{
    return m_selectedDevice;
}

void AVFCameraDeviceControl::setSelectedDevice(int index)
{
    if (index >= 0 &&
            index < deviceCount() &&
            index != m_selectedDevice) {
        m_dirty = true;
        m_selectedDevice = index;
        Q_EMIT selectedDeviceChanged(index);
        Q_EMIT selectedDeviceChanged(deviceName(index));
    }
}

AVCaptureDevice *AVFCameraDeviceControl::createCaptureDevice()
{
    m_dirty = false;
    AVCaptureDevice *device = 0;

    QString deviceId = deviceName(m_selectedDevice);
    if (!deviceId.isEmpty()) {
        device = [AVCaptureDevice deviceWithUniqueID:
                    [NSString stringWithUTF8String:
                        deviceId.toUtf8().constData()]];
    }

    if (!device)
        device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];

    return device;
}

#include "moc_avfcameradevicecontrol.cpp"
