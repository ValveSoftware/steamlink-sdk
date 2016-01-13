/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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
