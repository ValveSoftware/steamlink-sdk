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

#include "qandroidcamerafocuscontrol.h"

#include "qandroidcamerasession.h"
#include "androidcamera.h"

QT_BEGIN_NAMESPACE

static QRect adjustedArea(const QRectF &area)
{
    // Qt maps focus points in the range (0.0, 0.0) -> (1.0, 1.0)
    // Android maps focus points in the range (-1000, -1000) -> (1000, 1000)
    // Converts an area in Qt coordinates to Android coordinates
    return QRect(-1000 + qRound(area.x() * 2000),
                 -1000 + qRound(area.y() * 2000),
                 qRound(area.width() * 2000),
                 qRound(area.height() * 2000))
            .intersected(QRect(-1000, -1000, 2000, 2000));
}

QAndroidCameraFocusControl::QAndroidCameraFocusControl(QAndroidCameraSession *session)
    : QCameraFocusControl()
    , m_session(session)
    , m_focusMode(QCameraFocus::AutoFocus)
    , m_focusPointMode(QCameraFocus::FocusPointAuto)
    , m_actualFocusPoint(0.5, 0.5)
    , m_continuousPictureFocusSupported(false)
    , m_continuousVideoFocusSupported(false)
{
    connect(m_session, SIGNAL(opened()),
            this, SLOT(onCameraOpened()));
    connect(m_session, SIGNAL(captureModeChanged(QCamera::CaptureModes)),
            this, SLOT(onCameraCaptureModeChanged()));
}

QCameraFocus::FocusModes QAndroidCameraFocusControl::focusMode() const
{
    return m_focusMode;
}

void QAndroidCameraFocusControl::setFocusMode(QCameraFocus::FocusModes mode)
{
    if (!m_session->camera()) {
        setFocusModeHelper(mode);
        return;
    }

    if (isFocusModeSupported(mode)) {
        QString focusMode = QLatin1String("fixed");

        if (mode.testFlag(QCameraFocus::HyperfocalFocus)) {
            focusMode = QLatin1String("edof");
        } else if (mode.testFlag(QCameraFocus::ManualFocus)) {
            focusMode = QLatin1String("fixed");
        } else if (mode.testFlag(QCameraFocus::AutoFocus)) {
            focusMode = QLatin1String("auto");
        } else if (mode.testFlag(QCameraFocus::MacroFocus)) {
            focusMode = QLatin1String("macro");
        } else if (mode.testFlag(QCameraFocus::ContinuousFocus)) {
            if ((m_session->captureMode().testFlag(QCamera::CaptureVideo) && m_continuousVideoFocusSupported)
                    || !m_continuousPictureFocusSupported) {
                focusMode = QLatin1String("continuous-video");
            } else {
                focusMode = QLatin1String("continuous-picture");
            }
        } else if (mode.testFlag(QCameraFocus::InfinityFocus)) {
            focusMode = QLatin1String("infinity");
        }

        m_session->camera()->setFocusMode(focusMode);

        // reset focus position
        m_session->camera()->cancelAutoFocus();

        setFocusModeHelper(mode);
    }
}

bool QAndroidCameraFocusControl::isFocusModeSupported(QCameraFocus::FocusModes mode) const
{
    return m_session->camera() ? m_supportedFocusModes.contains(mode) : false;
}

QCameraFocus::FocusPointMode QAndroidCameraFocusControl::focusPointMode() const
{
    return m_focusPointMode;
}

void QAndroidCameraFocusControl::setFocusPointMode(QCameraFocus::FocusPointMode mode)
{
    if (!m_session->camera()) {
        setFocusPointModeHelper(mode);
        return;
    }

    if (isFocusPointModeSupported(mode)) {
        if (mode == QCameraFocus::FocusPointCustom) {
            m_actualFocusPoint = m_customFocusPoint;
        } else {
            // FocusPointAuto | FocusPointCenter
            // note: there is no way to know the actual focus point in FocusPointAuto mode,
            // so just report the focus point to be at the center of the frame
            m_actualFocusPoint = QPointF(0.5, 0.5);
        }

        setFocusPointModeHelper(mode);

        updateFocusZones();
        setCameraFocusArea();
    }
}

bool QAndroidCameraFocusControl::isFocusPointModeSupported(QCameraFocus::FocusPointMode mode) const
{
    return m_session->camera() ? m_supportedFocusPointModes.contains(mode) : false;
}

QPointF QAndroidCameraFocusControl::customFocusPoint() const
{
    return m_customFocusPoint;
}

void QAndroidCameraFocusControl::setCustomFocusPoint(const QPointF &point)
{
    if (m_customFocusPoint != point) {
        m_customFocusPoint = point;
        emit customFocusPointChanged(m_customFocusPoint);
    }

    if (m_session->camera() && m_focusPointMode == QCameraFocus::FocusPointCustom) {
        m_actualFocusPoint = m_customFocusPoint;
        updateFocusZones();
        setCameraFocusArea();
    }
}

QCameraFocusZoneList QAndroidCameraFocusControl::focusZones() const
{
    return m_focusZones;
}

void QAndroidCameraFocusControl::onCameraOpened()
{
    connect(m_session->camera(), SIGNAL(previewSizeChanged()),
            this, SLOT(onViewportSizeChanged()));
    connect(m_session->camera(), SIGNAL(autoFocusStarted()),
            this, SLOT(onAutoFocusStarted()));
    connect(m_session->camera(), SIGNAL(autoFocusComplete(bool)),
            this, SLOT(onAutoFocusComplete(bool)));

    m_supportedFocusModes.clear();
    m_continuousPictureFocusSupported = false;
    m_continuousVideoFocusSupported = false;
    m_supportedFocusPointModes.clear();

    QStringList focusModes = m_session->camera()->getSupportedFocusModes();
    for (int i = 0; i < focusModes.size(); ++i) {
        const QString &focusMode = focusModes.at(i);
        if (focusMode == QLatin1String("auto")) {
            m_supportedFocusModes << QCameraFocus::AutoFocus;
        } else if (focusMode == QLatin1String("continuous-picture")) {
            m_supportedFocusModes << QCameraFocus::ContinuousFocus;
            m_continuousPictureFocusSupported = true;
        } else if (focusMode == QLatin1String("continuous-video")) {
            m_supportedFocusModes << QCameraFocus::ContinuousFocus;
            m_continuousVideoFocusSupported = true;
        } else if (focusMode == QLatin1String("edof")) {
            m_supportedFocusModes << QCameraFocus::HyperfocalFocus;
        } else if (focusMode == QLatin1String("fixed")) {
            m_supportedFocusModes << QCameraFocus::ManualFocus;
        } else if (focusMode == QLatin1String("infinity")) {
            m_supportedFocusModes << QCameraFocus::InfinityFocus;
        } else if (focusMode == QLatin1String("macro")) {
            m_supportedFocusModes << QCameraFocus::MacroFocus;
        }
    }

    m_supportedFocusPointModes << QCameraFocus::FocusPointAuto;
    if (m_session->camera()->getMaxNumFocusAreas() > 0)
        m_supportedFocusPointModes << QCameraFocus::FocusPointCenter << QCameraFocus::FocusPointCustom;

    if (!m_supportedFocusModes.contains(m_focusMode))
        setFocusModeHelper(QCameraFocus::AutoFocus);
    if (!m_supportedFocusPointModes.contains(m_focusPointMode))
        setFocusPointModeHelper(QCameraFocus::FocusPointAuto);

    setFocusMode(m_focusMode);
    setCustomFocusPoint(m_customFocusPoint);
    setFocusPointMode(m_focusPointMode);
}

void QAndroidCameraFocusControl::updateFocusZones(QCameraFocusZone::FocusZoneStatus status)
{
    if (!m_session->camera())
        return;

    // create a focus zone (50x50 pixel) around the focus point
    m_focusZones.clear();

    if (!m_actualFocusPoint.isNull()) {
        QSize viewportSize = m_session->camera()->previewSize();

        if (!viewportSize.isValid())
            return;

        QSizeF focusSize(50.f / viewportSize.width(), 50.f / viewportSize.height());
        float x = qBound(qreal(0),
                         m_actualFocusPoint.x() - (focusSize.width() / 2),
                         1.f - focusSize.width());
        float y = qBound(qreal(0),
                         m_actualFocusPoint.y() - (focusSize.height() / 2),
                         1.f - focusSize.height());

        QRectF area(QPointF(x, y), focusSize);

        m_focusZones.append(QCameraFocusZone(area, status));
    }

    emit focusZonesChanged();
}

void QAndroidCameraFocusControl::setCameraFocusArea()
{
    QList<QRect> areas;
    if (m_focusPointMode != QCameraFocus::FocusPointAuto) {
        // in FocusPointAuto mode, leave the area list empty
        // to let the driver choose the focus point.

        for (int i = 0; i < m_focusZones.size(); ++i)
            areas.append(adjustedArea(m_focusZones.at(i).area()));

    }
    m_session->camera()->setFocusAreas(areas);
}

void QAndroidCameraFocusControl::onViewportSizeChanged()
{
    QCameraFocusZone::FocusZoneStatus status = QCameraFocusZone::Selected;
    if (!m_focusZones.isEmpty())
        status = m_focusZones.at(0).status();
    updateFocusZones(status);
    setCameraFocusArea();
}

void QAndroidCameraFocusControl::onCameraCaptureModeChanged()
{
    if (m_session->camera() && m_focusMode == QCameraFocus::ContinuousFocus) {
        QString focusMode;
        if ((m_session->captureMode().testFlag(QCamera::CaptureVideo) && m_continuousVideoFocusSupported)
                || !m_continuousPictureFocusSupported) {
            focusMode = QLatin1String("continuous-video");
        } else {
            focusMode = QLatin1String("continuous-picture");
        }
        m_session->camera()->setFocusMode(focusMode);
        m_session->camera()->cancelAutoFocus();
    }
}

void QAndroidCameraFocusControl::onAutoFocusStarted()
{
    updateFocusZones(QCameraFocusZone::Selected);
}

void QAndroidCameraFocusControl::onAutoFocusComplete(bool success)
{
    if (success)
        updateFocusZones(QCameraFocusZone::Focused);
}

QT_END_NAMESPACE
