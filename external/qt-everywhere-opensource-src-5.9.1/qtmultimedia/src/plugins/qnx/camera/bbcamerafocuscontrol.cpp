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
#include "bbcamerafocuscontrol.h"

#include "bbcamerasession.h"

#include <QDebug>

QT_BEGIN_NAMESPACE

BbCameraFocusControl::BbCameraFocusControl(BbCameraSession *session, QObject *parent)
    : QCameraFocusControl(parent)
    , m_session(session)
    , m_focusMode(QCameraFocus::FocusModes())
    , m_focusPointMode(QCameraFocus::FocusPointAuto)
    , m_customFocusPoint(QPointF(0, 0))
{
}

QCameraFocus::FocusModes BbCameraFocusControl::focusMode() const
{
    camera_focusmode_t focusMode = CAMERA_FOCUSMODE_OFF;

    const camera_error_t result = camera_get_focus_mode(m_session->handle(), &focusMode);
    if (result != CAMERA_EOK) {
        qWarning() << "Unable to retrieve focus mode from camera:" << result;
        return QCameraFocus::FocusModes();
    }

    switch (focusMode) {
    case CAMERA_FOCUSMODE_EDOF:
        return QCameraFocus::HyperfocalFocus;
    case CAMERA_FOCUSMODE_MANUAL:
        return QCameraFocus::ManualFocus;
    case CAMERA_FOCUSMODE_AUTO:
        return QCameraFocus::AutoFocus;
    case CAMERA_FOCUSMODE_MACRO:
        return QCameraFocus::MacroFocus;
    case CAMERA_FOCUSMODE_CONTINUOUS_AUTO:
        return QCameraFocus::ContinuousFocus;
    case CAMERA_FOCUSMODE_CONTINUOUS_MACRO: // fall through
    case CAMERA_FOCUSMODE_OFF: // fall through
    default:
            return QCameraFocus::FocusModes();
    }
}

void BbCameraFocusControl::setFocusMode(QCameraFocus::FocusModes mode)
{
    if (m_focusMode == mode)
        return;

    camera_focusmode_t focusMode = CAMERA_FOCUSMODE_OFF;

    if (mode == QCameraFocus::HyperfocalFocus)
        focusMode = CAMERA_FOCUSMODE_EDOF;
    else if (mode == QCameraFocus::ManualFocus)
        focusMode = CAMERA_FOCUSMODE_MANUAL;
    else if (mode == QCameraFocus::AutoFocus)
        focusMode = CAMERA_FOCUSMODE_AUTO;
    else if (mode == QCameraFocus::MacroFocus)
        focusMode = CAMERA_FOCUSMODE_MACRO;
    else if (mode == QCameraFocus::ContinuousFocus)
        focusMode = CAMERA_FOCUSMODE_CONTINUOUS_AUTO;

    const camera_error_t result = camera_set_focus_mode(m_session->handle(), focusMode);

    if (result != CAMERA_EOK) {
        qWarning() << "Unable to set focus mode:" << result;
        return;
    }

    m_focusMode = mode;
    emit focusModeChanged(m_focusMode);
}

bool BbCameraFocusControl::isFocusModeSupported(QCameraFocus::FocusModes mode) const
{
    if (m_session->state() == QCamera::UnloadedState)
        return false;

    if (mode == QCameraFocus::HyperfocalFocus)
        return false; //TODO how to check?
    else if (mode == QCameraFocus::ManualFocus)
        return camera_has_feature(m_session->handle(), CAMERA_FEATURE_MANUALFOCUS);
    else if (mode == QCameraFocus::AutoFocus)
        return camera_has_feature(m_session->handle(), CAMERA_FEATURE_AUTOFOCUS);
    else if (mode == QCameraFocus::MacroFocus)
        return camera_has_feature(m_session->handle(), CAMERA_FEATURE_MACROFOCUS);
    else if (mode == QCameraFocus::ContinuousFocus)
        return camera_has_feature(m_session->handle(), CAMERA_FEATURE_AUTOFOCUS);

    return false;
}

QCameraFocus::FocusPointMode BbCameraFocusControl::focusPointMode() const
{
    return m_focusPointMode;
}

void BbCameraFocusControl::setFocusPointMode(QCameraFocus::FocusPointMode mode)
{
    if (m_session->status() != QCamera::ActiveStatus)
        return;

    if (m_focusPointMode == mode)
        return;

    m_focusPointMode = mode;
    emit focusPointModeChanged(m_focusPointMode);

    if (m_focusPointMode == QCameraFocus::FocusPointAuto) {
        //TODO: is this correct?
        const camera_error_t result = camera_set_focus_regions(m_session->handle(), 0, 0);
        if (result != CAMERA_EOK) {
            qWarning() << "Unable to set focus region:" << result;
            return;
        }

        emit focusZonesChanged();
    } else if (m_focusPointMode == QCameraFocus::FocusPointCenter) {
        // get the size of the viewfinder
        int viewfinderWidth = 0;
        int viewfinderHeight = 0;

        if (!retrieveViewfinderSize(&viewfinderWidth, &viewfinderHeight))
            return;

        // define a 40x40 pixel focus region in the center of the viewfinder
        camera_region_t focusRegion;
        focusRegion.left = (viewfinderWidth / 2) - 20;
        focusRegion.top = (viewfinderHeight / 2) - 20;
        focusRegion.width = 40;
        focusRegion.height = 40;

        camera_error_t result = camera_set_focus_regions(m_session->handle(), 1, &focusRegion);
        if (result != CAMERA_EOK) {
            qWarning() << "Unable to set focus region:" << result;
            return;
        }

        // re-set focus mode to apply focus region changes
        camera_focusmode_t focusMode = CAMERA_FOCUSMODE_OFF;
        result = camera_get_focus_mode(m_session->handle(), &focusMode);
        camera_set_focus_mode(m_session->handle(), focusMode);

        emit focusZonesChanged();

    } else if (m_focusPointMode == QCameraFocus::FocusPointFaceDetection) {
        //TODO: implement later
    } else if (m_focusPointMode == QCameraFocus::FocusPointCustom) {
        updateCustomFocusRegion();
    }
}

bool BbCameraFocusControl::isFocusPointModeSupported(QCameraFocus::FocusPointMode mode) const
{
    if (m_session->state() == QCamera::UnloadedState)
        return false;

    if (mode == QCameraFocus::FocusPointAuto) {
        return camera_has_feature(m_session->handle(), CAMERA_FEATURE_AUTOFOCUS);
    } else if (mode == QCameraFocus::FocusPointCenter) {
        return camera_has_feature(m_session->handle(), CAMERA_FEATURE_REGIONFOCUS);
    } else if (mode == QCameraFocus::FocusPointFaceDetection) {
        return false; //TODO: implement via custom region in combination with face detection in viewfinder
    } else if (mode == QCameraFocus::FocusPointCustom) {
        return camera_has_feature(m_session->handle(), CAMERA_FEATURE_REGIONFOCUS);
    }

    return false;
}

QPointF BbCameraFocusControl::customFocusPoint() const
{
    return m_customFocusPoint;
}

void BbCameraFocusControl::setCustomFocusPoint(const QPointF &point)
{
    if (m_customFocusPoint == point)
        return;

    m_customFocusPoint = point;
    emit customFocusPointChanged(m_customFocusPoint);

    updateCustomFocusRegion();
}

QCameraFocusZoneList BbCameraFocusControl::focusZones() const
{
    if (m_session->state() == QCamera::UnloadedState)
        return QCameraFocusZoneList();

    camera_region_t regions[20];
    int supported = 0;
    int asked = 0;
    camera_error_t result = camera_get_focus_regions(m_session->handle(), 20, &supported, &asked, regions);

    if (result != CAMERA_EOK) {
        qWarning() << "Unable to retrieve focus regions:" << result;
        return QCameraFocusZoneList();
    }

    // retrieve width and height of viewfinder
    int viewfinderWidth = 0;
    int viewfinderHeight = 0;
    if (m_session->captureMode() & QCamera::CaptureStillImage)
        result = camera_get_photovf_property(m_session->handle(),
                                             CAMERA_IMGPROP_WIDTH, &viewfinderWidth,
                                             CAMERA_IMGPROP_HEIGHT, &viewfinderHeight);
    else if (m_session->captureMode() & QCamera::CaptureVideo)
        result = camera_get_videovf_property(m_session->handle(),
                                             CAMERA_IMGPROP_WIDTH, &viewfinderWidth,
                                             CAMERA_IMGPROP_HEIGHT, &viewfinderHeight);

    if (result != CAMERA_EOK) {
        qWarning() << "Unable to retrieve viewfinder size:" << result;
        return QCameraFocusZoneList();
    }

    QCameraFocusZoneList list;
    for (int i = 0; i < asked; ++i) {
        const int x = regions[i].left;
        const int y = regions[i].top;
        const int width = regions[i].width;
        const int height = regions[i].height;

        QRectF rect(static_cast<float>(x)/static_cast<float>(viewfinderWidth),
                    static_cast<float>(y)/static_cast<float>(viewfinderHeight),
                    static_cast<float>(width)/static_cast<float>(viewfinderWidth),
                    static_cast<float>(height)/static_cast<float>(viewfinderHeight));

        list << QCameraFocusZone(rect, QCameraFocusZone::Focused); //TODO: how to know if a zone is unused/selected/focused?!?
    }

    return list;
}

void BbCameraFocusControl::updateCustomFocusRegion()
{
    // get the size of the viewfinder
    int viewfinderWidth = 0;
    int viewfinderHeight = 0;

    if (!retrieveViewfinderSize(&viewfinderWidth, &viewfinderHeight))
        return;

    // define a 40x40 pixel focus region around the custom focus point
    camera_region_t focusRegion;
    focusRegion.left = qMax(0, static_cast<int>(m_customFocusPoint.x() * viewfinderWidth) - 20);
    focusRegion.top = qMax(0, static_cast<int>(m_customFocusPoint.y() * viewfinderHeight) - 20);
    focusRegion.width = 40;
    focusRegion.height = 40;

    camera_error_t result = camera_set_focus_regions(m_session->handle(), 1, &focusRegion);
    if (result != CAMERA_EOK) {
        qWarning() << "Unable to set focus region:" << result;
        return;
    }

    // re-set focus mode to apply focus region changes
    camera_focusmode_t focusMode = CAMERA_FOCUSMODE_OFF;
    result = camera_get_focus_mode(m_session->handle(), &focusMode);
    camera_set_focus_mode(m_session->handle(), focusMode);

    emit focusZonesChanged();
}

bool BbCameraFocusControl::retrieveViewfinderSize(int *width, int *height)
{
    if (!width || !height)
        return false;

    camera_error_t result = CAMERA_EOK;
    if (m_session->captureMode() & QCamera::CaptureStillImage)
        result = camera_get_photovf_property(m_session->handle(),
                                             CAMERA_IMGPROP_WIDTH, width,
                                             CAMERA_IMGPROP_HEIGHT, height);
    else if (m_session->captureMode() & QCamera::CaptureVideo)
        result = camera_get_videovf_property(m_session->handle(),
                                             CAMERA_IMGPROP_WIDTH, width,
                                             CAMERA_IMGPROP_HEIGHT, height);

    if (result != CAMERA_EOK) {
        qWarning() << "Unable to retrieve viewfinder size:" << result;
        return false;
    }

    return true;
}

QT_END_NAMESPACE
