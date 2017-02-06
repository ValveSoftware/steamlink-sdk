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


#ifndef CAMERABINCONTROL_H
#define CAMERABINCONTROL_H

#include <QHash>
#include <qcameracontrol.h>
#include "camerabinsession.h"

QT_BEGIN_NAMESPACE

class CamerabinResourcePolicy;

class CameraBinControl : public QCameraControl
{
    Q_OBJECT
    Q_PROPERTY(bool viewfinderColorSpaceConversion READ viewfinderColorSpaceConversion WRITE setViewfinderColorSpaceConversion)
public:
    CameraBinControl( CameraBinSession *session );
    virtual ~CameraBinControl();

    bool isValid() const { return true; }

    QCamera::State state() const;
    void setState(QCamera::State state);

    QCamera::Status status() const;

    QCamera::CaptureModes captureMode() const;
    void setCaptureMode(QCamera::CaptureModes mode);

    bool isCaptureModeSupported(QCamera::CaptureModes mode) const;
    bool canChangeProperty(PropertyChangeType changeType, QCamera::Status status) const;
    bool viewfinderColorSpaceConversion() const;

    CamerabinResourcePolicy *resourcePolicy() { return m_resourcePolicy; }

public slots:
    void reloadLater();
    void setViewfinderColorSpaceConversion(bool enabled);

private slots:
    void delayedReload();

    void handleResourcesGranted();
    void handleResourcesLost();

    void handleBusyChanged(bool);
    void handleCameraError(int error, const QString &errorString);

private:
    void updateSupportedResolutions(const QString &device);

    CameraBinSession *m_session;
    QCamera::State m_state;
    CamerabinResourcePolicy *m_resourcePolicy;

    bool m_reloadPending;
};

QT_END_NAMESPACE

#endif // CAMERABINCONTROL_H
