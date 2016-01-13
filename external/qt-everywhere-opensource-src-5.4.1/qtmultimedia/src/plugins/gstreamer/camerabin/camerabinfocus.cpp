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

#include "camerabinfocus.h"
#include "camerabinsession.h"

#include <gst/interfaces/photography.h>

#include <QDebug>
#include <QtCore/qmetaobject.h>

//#define CAMERABIN_DEBUG 1

QT_BEGIN_NAMESPACE

CameraBinFocus::CameraBinFocus(CameraBinSession *session)
    :QCameraFocusControl(session),
     m_session(session),
     m_cameraState(QCamera::UnloadedState),
     m_focusMode(QCameraFocus::AutoFocus),
     m_focusPointMode(QCameraFocus::FocusPointAuto),
     m_focusStatus(QCamera::Unlocked),
     m_focusZoneStatus(QCameraFocusZone::Selected),
     m_focusPoint(0.5, 0.5),
     m_focusRect(0, 0, 0.3, 0.3)
{
    m_focusRect.moveCenter(m_focusPoint);

    gst_photography_set_focus_mode(m_session->photography(), GST_PHOTOGRAPHY_FOCUS_MODE_AUTO);

    connect(m_session, SIGNAL(stateChanged(QCamera::State)),
            this, SLOT(_q_handleCameraStateChange(QCamera::State)));
}

CameraBinFocus::~CameraBinFocus()
{
}

QCameraFocus::FocusModes CameraBinFocus::focusMode() const
{
    return m_focusMode;
}

void CameraBinFocus::setFocusMode(QCameraFocus::FocusModes mode)
{
    GstFocusMode photographyMode;

    switch (mode) {
    case QCameraFocus::AutoFocus:
        photographyMode = GST_PHOTOGRAPHY_FOCUS_MODE_AUTO;
        break;
    case QCameraFocus::HyperfocalFocus:
        photographyMode = GST_PHOTOGRAPHY_FOCUS_MODE_HYPERFOCAL;
        break;
    case QCameraFocus::InfinityFocus:
        photographyMode = GST_PHOTOGRAPHY_FOCUS_MODE_INFINITY;
        break;
    case QCameraFocus::ContinuousFocus:
        photographyMode = GST_PHOTOGRAPHY_FOCUS_MODE_CONTINUOUS_NORMAL;
        break;
    case QCameraFocus::MacroFocus:
        photographyMode = GST_PHOTOGRAPHY_FOCUS_MODE_MACRO;
        break;
    default:
        if (mode & QCameraFocus::AutoFocus) {
            photographyMode = GST_PHOTOGRAPHY_FOCUS_MODE_AUTO;
            break;
        } else {
            return;
        }
    }

    if (gst_photography_set_focus_mode(m_session->photography(), photographyMode))
        m_focusMode = mode;
}

bool CameraBinFocus::isFocusModeSupported(QCameraFocus::FocusModes mode) const
{
    switch (mode) {
    case QCameraFocus::AutoFocus:
    case QCameraFocus::HyperfocalFocus:
    case QCameraFocus::InfinityFocus:
    case QCameraFocus::ContinuousFocus:
    case QCameraFocus::MacroFocus:
        return true;
    default:
        return mode & QCameraFocus::AutoFocus;
    }
}

QCameraFocus::FocusPointMode CameraBinFocus::focusPointMode() const
{
    return m_focusPointMode;
}

void CameraBinFocus::setFocusPointMode(QCameraFocus::FocusPointMode mode)
{
    Q_UNUSED(mode);
    if (m_focusPointMode != mode
            && (mode == QCameraFocus::FocusPointAuto || mode == QCameraFocus::FocusPointCustom)) {
        m_focusPointMode = mode;

        if (m_focusPointMode == QCameraFocus::FocusPointAuto)
            resetFocusPoint();

        emit focusPointModeChanged(m_focusPointMode);

    }
}

bool CameraBinFocus::isFocusPointModeSupported(QCameraFocus::FocusPointMode mode) const
{
    return mode == QCameraFocus::FocusPointAuto || mode == QCameraFocus::FocusPointCustom;
}

QPointF CameraBinFocus::customFocusPoint() const
{
    return m_focusPoint;
}

void CameraBinFocus::setCustomFocusPoint(const QPointF &point)
{
    if (m_focusPoint != point) {
        m_focusPoint = point;

        // Bound the focus point so the focus rect remains entirely within the unit square.
        m_focusPoint.setX(qBound(m_focusRect.width() / 2, m_focusPoint.x(), 1 - m_focusRect.width() / 2));
        m_focusPoint.setY(qBound(m_focusRect.height() / 2, m_focusPoint.y(), 1 - m_focusRect.height() / 2));

        if (m_focusPointMode == QCameraFocus::FocusPointCustom) {
            const QRectF focusRect = m_focusRect;
            m_focusRect.moveCenter(m_focusPoint);

            updateRegionOfInterest(m_focusRect, 1);

            if (focusRect != m_focusRect) {
                emit focusZonesChanged();
            }
        }

        emit customFocusPointChanged(m_focusPoint);
    }
}

QCameraFocusZoneList CameraBinFocus::focusZones() const
{
    return QCameraFocusZoneList() << QCameraFocusZone(m_focusRect, m_focusZoneStatus);
}


void CameraBinFocus::handleFocusMessage(GstMessage *gm)
{
    //it's a sync message, so it's called from non main thread
    if (gst_structure_has_name(gm->structure, GST_PHOTOGRAPHY_AUTOFOCUS_DONE)) {
        gint status = GST_PHOTOGRAPHY_FOCUS_STATUS_NONE;
        gst_structure_get_int (gm->structure, "status", &status);
        QCamera::LockStatus focusStatus = m_focusStatus;
        QCamera::LockChangeReason reason = QCamera::UserRequest;

        switch (status) {
            case GST_PHOTOGRAPHY_FOCUS_STATUS_FAIL:
                focusStatus = QCamera::Unlocked;
                reason = QCamera::LockFailed;
                break;
            case GST_PHOTOGRAPHY_FOCUS_STATUS_SUCCESS:
                focusStatus = QCamera::Locked;
                break;
            case GST_PHOTOGRAPHY_FOCUS_STATUS_NONE:
                break;
            case GST_PHOTOGRAPHY_FOCUS_STATUS_RUNNING:
                focusStatus = QCamera::Searching;
                break;
            default:
                break;
        }

        static int signalIndex = metaObject()->indexOfSlot(
                    "_q_setFocusStatus(QCamera::LockStatus,QCamera::LockChangeReason)");
        metaObject()->method(signalIndex).invoke(this,
                                                 Qt::QueuedConnection,
                                                 Q_ARG(QCamera::LockStatus,focusStatus),
                                                 Q_ARG(QCamera::LockChangeReason,reason));
    }
}

void CameraBinFocus::_q_setFocusStatus(QCamera::LockStatus status, QCamera::LockChangeReason reason)
{
#ifdef CAMERABIN_DEBUG
    qDebug() << Q_FUNC_INFO << "Current:"
                << m_focusStatus
                << "New:"
                << status << reason;
#endif

    if (m_focusStatus != status) {
        m_focusStatus = status;

        QCameraFocusZone::FocusZoneStatus zonesStatus =
                m_focusStatus == QCamera::Locked ?
                    QCameraFocusZone::Focused : QCameraFocusZone::Selected;

        if (m_focusZoneStatus != zonesStatus) {
            m_focusZoneStatus = zonesStatus;
            emit focusZonesChanged();
        }

        emit _q_focusStatusChanged(m_focusStatus, reason);
    }
}

void CameraBinFocus::_q_handleCameraStateChange(QCamera::State state)
{
    m_cameraState = state;
    if (state == QCamera::ActiveState) {
        if (GstPad *pad = gst_element_get_static_pad(m_session->cameraSource(), "vfsrc")) {
            if (GstCaps *caps = gst_pad_get_negotiated_caps(pad)) {
                if (GstStructure *structure = gst_caps_get_structure(caps, 0)) {
                    int width = 0;
                    int height = 0;
                    gst_structure_get_int(structure, "width", &width);
                    gst_structure_get_int(structure, "height", &height);
                    setViewfinderResolution(QSize(width, height));
                }
                gst_caps_unref(caps);
            }
            gst_object_unref(GST_OBJECT(pad));
        }
        if (m_focusPointMode == QCameraFocus::FocusPointCustom) {
                updateRegionOfInterest(m_focusRect, 1);
        }
    } else {
        _q_setFocusStatus(QCamera::Unlocked, QCamera::LockLost);

        resetFocusPoint();
    }
}

void CameraBinFocus::_q_startFocusing()
{
    _q_setFocusStatus(QCamera::Searching, QCamera::UserRequest);
    gst_photography_set_autofocus(m_session->photography(), TRUE);
}

void CameraBinFocus::_q_stopFocusing()
{
    gst_photography_set_autofocus(m_session->photography(), FALSE);
    _q_setFocusStatus(QCamera::Unlocked, QCamera::UserRequest);
}

void CameraBinFocus::setViewfinderResolution(const QSize &resolution)
{
    if (resolution != m_viewfinderResolution) {
        m_viewfinderResolution = resolution;
        if (!resolution.isEmpty()) {
            const QPointF center = m_focusRect.center();
            m_focusRect.setWidth(m_focusRect.height() * resolution.height() / resolution.width());
            m_focusRect.moveCenter(center);
        }
    }
}

void CameraBinFocus::resetFocusPoint()
{
    const QRectF focusRect = m_focusRect;
    m_focusPoint = QPointF(0.5, 0.5);
    m_focusRect.moveCenter(m_focusPoint);

    updateRegionOfInterest(QRectF(0, 0, 0, 0), 0);

    if (focusRect != m_focusRect) {
        emit customFocusPointChanged(m_focusPoint);
        emit focusZonesChanged();
    }
}

void CameraBinFocus::updateRegionOfInterest(const QRectF &focusRect, int priority)
{
    if (m_cameraState != QCamera::ActiveState)
        return;

    GstElement * const cameraSource = m_session->cameraSource();
    if (!cameraSource)
        return;

    GstStructure *region = gst_structure_new(
                "region",
                "region-x"        , G_TYPE_UINT , uint(m_viewfinderResolution.width()  * focusRect.x()),
                "region-y"        , G_TYPE_UINT,  uint(m_viewfinderResolution.height() * focusRect.y()),
                "region-w"        , G_TYPE_UINT , uint(m_viewfinderResolution.width()  * focusRect.width()),
                "region-h"        , G_TYPE_UINT,  uint(m_viewfinderResolution.height() * focusRect.height()),
                "region-priority" , G_TYPE_UINT,  priority,
                NULL);

    GValue regionValue = G_VALUE_INIT;
    g_value_init(&regionValue, GST_TYPE_STRUCTURE);
    gst_value_set_structure(&regionValue, region);
    gst_structure_free(region);

    GValue regions = G_VALUE_INIT;
    g_value_init(&regions, GST_TYPE_LIST);
    gst_value_list_append_value(&regions, &regionValue);
    g_value_unset(&regionValue);

    GstStructure *regionsOfInterest = gst_structure_new(
                "regions-of-interest",
                "frame-width"     , G_TYPE_UINT , m_viewfinderResolution.width(),
                "frame-height"    , G_TYPE_UINT,  m_viewfinderResolution.height(),
                NULL);
    gst_structure_set_value(regionsOfInterest, "regions", &regions);
    g_value_unset(&regions);

    GstEvent *event = gst_event_new_custom(GST_EVENT_CUSTOM_UPSTREAM, regionsOfInterest);
    gst_element_send_event(cameraSource, event);
}

QT_END_NAMESPACE
