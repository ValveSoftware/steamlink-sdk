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

#include "qgstreamervideoinputdevicecontrol_p.h"

#include <QtCore/QDir>
#include <QtCore/QDebug>

#include <private/qgstutils_p.h>

QGstreamerVideoInputDeviceControl::QGstreamerVideoInputDeviceControl(QObject *parent)
    :QVideoDeviceSelectorControl(parent), m_factory(0), m_selectedDevice(0)
{
}

QGstreamerVideoInputDeviceControl::QGstreamerVideoInputDeviceControl(
        GstElementFactory *factory, QObject *parent)
    : QVideoDeviceSelectorControl(parent), m_factory(factory), m_selectedDevice(0)
{
    if (m_factory)
        gst_object_ref(GST_OBJECT(m_factory));
}

QGstreamerVideoInputDeviceControl::~QGstreamerVideoInputDeviceControl()
{
    if (m_factory)
        gst_object_unref(GST_OBJECT(m_factory));
}

int QGstreamerVideoInputDeviceControl::deviceCount() const
{
    return QGstUtils::enumerateCameras(m_factory).count();
}

QString QGstreamerVideoInputDeviceControl::deviceName(int index) const
{
    return QGstUtils::enumerateCameras(m_factory).value(index).name;
}

QString QGstreamerVideoInputDeviceControl::deviceDescription(int index) const
{
    return QGstUtils::enumerateCameras(m_factory).value(index).description;
}

int QGstreamerVideoInputDeviceControl::defaultDevice() const
{
    return 0;
}

int QGstreamerVideoInputDeviceControl::selectedDevice() const
{
    return m_selectedDevice;
}

void QGstreamerVideoInputDeviceControl::setSelectedDevice(int index)
{
    if (index != m_selectedDevice) {
        m_selectedDevice = index;
        emit selectedDeviceChanged(index);
        emit selectedDeviceChanged(deviceName(index));
    }
}
