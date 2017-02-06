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
#ifndef BBCAMERAFOCUSCONTROL_H
#define BBCAMERAFOCUSCONTROL_H

#include <qcamerafocuscontrol.h>

QT_BEGIN_NAMESPACE

class BbCameraSession;

class BbCameraFocusControl : public QCameraFocusControl
{
    Q_OBJECT
public:
    explicit BbCameraFocusControl(BbCameraSession *session, QObject *parent = 0);

    QCameraFocus::FocusModes focusMode() const Q_DECL_OVERRIDE;
    void setFocusMode(QCameraFocus::FocusModes mode) Q_DECL_OVERRIDE;
    bool isFocusModeSupported(QCameraFocus::FocusModes mode) const Q_DECL_OVERRIDE;
    QCameraFocus::FocusPointMode focusPointMode() const Q_DECL_OVERRIDE;
    void setFocusPointMode(QCameraFocus::FocusPointMode mode) Q_DECL_OVERRIDE;
    bool isFocusPointModeSupported(QCameraFocus::FocusPointMode mode) const Q_DECL_OVERRIDE;
    QPointF customFocusPoint() const Q_DECL_OVERRIDE;
    void setCustomFocusPoint(const QPointF &point) Q_DECL_OVERRIDE;
    QCameraFocusZoneList focusZones() const Q_DECL_OVERRIDE;

private:
    void updateCustomFocusRegion();
    bool retrieveViewfinderSize(int *width, int *height);

    BbCameraSession *m_session;

    QCameraFocus::FocusModes m_focusMode;
    QCameraFocus::FocusPointMode m_focusPointMode;
    QPointF m_customFocusPoint;
};

QT_END_NAMESPACE

#endif
