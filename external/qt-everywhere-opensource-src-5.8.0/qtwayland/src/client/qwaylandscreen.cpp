/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qwaylandscreen_p.h"

#include "qwaylanddisplay_p.h"
#include "qwaylandcursor_p.h"
#include "qwaylandwindow_p.h"

#include <QtGui/QGuiApplication>

#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformwindow.h>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandScreen::QWaylandScreen(QWaylandDisplay *waylandDisplay, int version, uint32_t id)
    : QPlatformScreen()
    , QtWayland::wl_output(waylandDisplay->wl_registry(), id, qMin(version, 2))
    , m_outputId(id)
    , mWaylandDisplay(waylandDisplay)
    , mScale(1)
    , mDepth(32)
    , mRefreshRate(60000)
    , mTransform(-1)
    , mFormat(QImage::Format_ARGB32_Premultiplied)
    , mOutputName(QStringLiteral("Screen%1").arg(id))
    , m_orientation(Qt::PrimaryOrientation)
    , mWaylandCursor(0)
{
}

QWaylandScreen::~QWaylandScreen()
{
    delete mWaylandCursor;
}

void QWaylandScreen::init()
{
    mWaylandCursor = new QWaylandCursor(this);
}

QWaylandDisplay * QWaylandScreen::display() const
{
    return mWaylandDisplay;
}

QRect QWaylandScreen::geometry() const
{
    // Scale geometry for QScreen. This makes window and screen
    // geometry be in the same coordinate system.
    return QRect(mGeometry.topLeft(), mGeometry.size() / mScale);
}

int QWaylandScreen::depth() const
{
    return mDepth;
}

QImage::Format QWaylandScreen::format() const
{
    return mFormat;
}

QSizeF QWaylandScreen::physicalSize() const
{
    if (mPhysicalSize.isEmpty())
        return QPlatformScreen::physicalSize();
    else
        return mPhysicalSize;
}

QDpi QWaylandScreen::logicalDpi() const
{
    static int force_dpi = !qgetenv("QT_WAYLAND_FORCE_DPI").isEmpty() ? qgetenv("QT_WAYLAND_FORCE_DPI").toInt() : -1;
    if (force_dpi > 0)
        return QDpi(force_dpi, force_dpi);

    return QPlatformScreen::logicalDpi();
}

QList<QPlatformScreen *> QWaylandScreen::virtualSiblings() const
{
    QList<QPlatformScreen *> list;
    const QList<QWaylandScreen*> screens = mWaylandDisplay->screens();
    list.reserve(screens.count());
    foreach (QWaylandScreen *screen, screens)
        list << screen;
    return list;
}

void QWaylandScreen::setOrientationUpdateMask(Qt::ScreenOrientations mask)
{
    foreach (QWindow *window, QGuiApplication::allWindows()) {
        QWaylandWindow *w = static_cast<QWaylandWindow *>(window->handle());
        if (w && w->screen() == this)
            w->setOrientationMask(mask);
    }
}

Qt::ScreenOrientation QWaylandScreen::orientation() const
{
    return m_orientation;
}

int QWaylandScreen::scale() const
{
    return mScale;
}

qreal QWaylandScreen::devicePixelRatio() const
{
    return qreal(mScale);
}

qreal QWaylandScreen::refreshRate() const
{
    return mRefreshRate / 1000.f;
}

QPlatformCursor *QWaylandScreen::cursor() const
{
    return  mWaylandCursor;
}

QWaylandScreen * QWaylandScreen::waylandScreenFromWindow(QWindow *window)
{
    QPlatformScreen *platformScreen = QPlatformScreen::platformScreenForWindow(window);
    return static_cast<QWaylandScreen *>(platformScreen);
}

void QWaylandScreen::output_mode(uint32_t flags, int width, int height, int refresh)
{
    if (!(flags & WL_OUTPUT_MODE_CURRENT))
        return;

    QSize size(width, height);

    if (size != mGeometry.size())
        mGeometry.setSize(size);

    if (refresh != mRefreshRate)
        mRefreshRate = refresh;
}

void QWaylandScreen::output_geometry(int32_t x, int32_t y,
                                     int32_t width, int32_t height,
                                     int subpixel,
                                     const QString &make,
                                     const QString &model,
                                     int32_t transform)
{
    Q_UNUSED(subpixel);
    Q_UNUSED(make);

    mTransform = transform;

    if (!model.isEmpty())
        mOutputName = model;

    mPhysicalSize = QSize(width, height);
    mGeometry.moveTopLeft(QPoint(x, y));
}

void QWaylandScreen::output_scale(int32_t factor)
{
    mScale = factor;
}

void QWaylandScreen::output_done()
{
    // the done event is sent after all the geometry and the mode events are sent,
    // and the last mode event to be sent is the active one, so we can trust the
    // values of mGeometry and mRefreshRate here

    if (mTransform >= 0) {
        bool isPortrait = mGeometry.height() > mGeometry.width();
        switch (mTransform) {
            case WL_OUTPUT_TRANSFORM_NORMAL:
                m_orientation = isPortrait ? Qt::PortraitOrientation : Qt::LandscapeOrientation;
                break;
            case WL_OUTPUT_TRANSFORM_90:
                m_orientation = isPortrait ? Qt::InvertedLandscapeOrientation : Qt::PortraitOrientation;
                break;
            case WL_OUTPUT_TRANSFORM_180:
                m_orientation = isPortrait ? Qt::InvertedPortraitOrientation : Qt::InvertedLandscapeOrientation;
                break;
            case WL_OUTPUT_TRANSFORM_270:
                m_orientation = isPortrait ? Qt::LandscapeOrientation : Qt::InvertedPortraitOrientation;
                break;
            // Ignore these ones, at least for now
            case WL_OUTPUT_TRANSFORM_FLIPPED:
            case WL_OUTPUT_TRANSFORM_FLIPPED_90:
            case WL_OUTPUT_TRANSFORM_FLIPPED_180:
            case WL_OUTPUT_TRANSFORM_FLIPPED_270:
                break;
        }

        QWindowSystemInterface::handleScreenOrientationChange(screen(), m_orientation);
        mTransform = -1;
    }
    QWindowSystemInterface::handleScreenGeometryChange(screen(), mGeometry, mGeometry);
    QWindowSystemInterface::handleScreenRefreshRateChange(screen(), refreshRate());
}

}

QT_END_NAMESPACE
