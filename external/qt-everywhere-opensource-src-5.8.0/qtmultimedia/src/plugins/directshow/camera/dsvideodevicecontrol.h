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

#ifndef DSVIDEODEVICECONTROL_H
#define DSVIDEODEVICECONTROL_H

#include <qvideodeviceselectorcontrol.h>
#include <QStringList>

QT_BEGIN_NAMESPACE
class DSCameraSession;

//QTM_USE_NAMESPACE

typedef QPair<QByteArray, QString> DSVideoDeviceInfo;

class DSVideoDeviceControl : public QVideoDeviceSelectorControl
{
    Q_OBJECT
public:
    DSVideoDeviceControl(QObject *parent = 0);

    int deviceCount() const;
    QString deviceName(int index) const;
    QString deviceDescription(int index) const;
    int defaultDevice() const;
    int selectedDevice() const;

    static const QList<DSVideoDeviceInfo> &availableDevices();

public Q_SLOTS:
    void setSelectedDevice(int index);

private:
    static void updateDevices();

    DSCameraSession* m_session;
    int selected;
};

QT_END_NAMESPACE

#endif
