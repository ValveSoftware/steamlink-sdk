/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
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
#ifndef BBVIDEODEVICESELECTORCONTROL_H
#define BBVIDEODEVICESELECTORCONTROL_H

#include <qvideodeviceselectorcontrol.h>
#include <QStringList>

QT_BEGIN_NAMESPACE

class BbCameraSession;

class BbVideoDeviceSelectorControl : public QVideoDeviceSelectorControl
{
    Q_OBJECT
public:
    explicit BbVideoDeviceSelectorControl(BbCameraSession *session, QObject *parent = 0);

    int deviceCount() const Q_DECL_OVERRIDE;
    QString deviceName(int index) const Q_DECL_OVERRIDE;
    QString deviceDescription(int index) const Q_DECL_OVERRIDE;
    int defaultDevice() const Q_DECL_OVERRIDE;
    int selectedDevice() const Q_DECL_OVERRIDE;

    static void enumerateDevices(QList<QByteArray> *devices, QStringList *descriptions);

public Q_SLOTS:
    void setSelectedDevice(int index) Q_DECL_OVERRIDE;

private:
    BbCameraSession* m_session;

    QList<QByteArray> m_devices;
    QStringList m_descriptions;

    int m_default;
    int m_selected;
};

QT_END_NAMESPACE

#endif
