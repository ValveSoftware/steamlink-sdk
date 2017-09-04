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

#include "qwaylandtouch_p.h"
#include "qwaylandinputdevice_p.h"
#include "qwaylanddisplay_p.h"

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandTouchExtension::QWaylandTouchExtension(QWaylandDisplay *display, uint32_t id)
    : QtWayland::qt_touch_extension(display->wl_registry(), id, 1),
      mDisplay(display),
      mTouchDevice(0),
      mPointsLeft(0),
      mFlags(0),
      mMouseSourceId(-1),
      mInputDevice(0)
{
}

void QWaylandTouchExtension::registerDevice(int caps)
{
    mTouchDevice = new QTouchDevice;
    mTouchDevice->setType(QTouchDevice::TouchScreen);
    mTouchDevice->setCapabilities(QTouchDevice::Capabilities(caps));
    QWindowSystemInterface::registerTouchDevice(mTouchDevice);
}

static inline qreal fromFixed(int f)
{
    return f / qreal(10000);
}

void QWaylandTouchExtension::touch_extension_touch(uint32_t time,
                                                   uint32_t id, uint32_t state, int32_t x, int32_t y,
                                                   int32_t normalized_x, int32_t normalized_y,
                                                   int32_t width, int32_t height, uint32_t pressure,
                                                   int32_t velocity_x, int32_t velocity_y,
                                                   uint32_t flags, wl_array *rawdata)
{
    if (!mInputDevice) {
        QList<QWaylandInputDevice *> inputDevices = mDisplay->inputDevices();
        if (inputDevices.isEmpty()) {
            qWarning("qt_touch_extension: handle_touch: No input devices");
            return;
        }
        mInputDevice = inputDevices.first();
    }
    QWaylandWindow *win = mInputDevice->touchFocus();
    if (!win)
        win = mInputDevice->pointerFocus();
    if (!win)
        win = mInputDevice->keyboardFocus();
    if (!win || !win->window()) {
        qWarning("qt_touch_extension: handle_touch: No pointer focus");
        return;
    }
    mTargetWindow = win->window();

    QWindowSystemInterface::TouchPoint tp;
    tp.id = id;
    tp.state = Qt::TouchPointState(int(state & 0xFFFF));
    int sentPointCount = state >> 16;
    if (!mPointsLeft) {
        Q_ASSERT(sentPointCount > 0);
        mPointsLeft = sentPointCount;
    }
    tp.flags = QTouchEvent::TouchPoint::InfoFlags(int(flags & 0xFFFF));

    if (!mTouchDevice)
        registerDevice(flags >> 16);

    tp.area = QRectF(0, 0, fromFixed(width), fromFixed(height));
    // Got surface-relative coords but need a (virtual) screen position.
    QPointF relPos = QPointF(fromFixed(x), fromFixed(y));
    QPointF delta = relPos - relPos.toPoint();
    tp.area.moveCenter(mTargetWindow->mapToGlobal(relPos.toPoint()) + delta);

    tp.normalPosition.setX(fromFixed(normalized_x));
    tp.normalPosition.setY(fromFixed(normalized_y));
    tp.pressure = pressure / 255.0;
    tp.velocity.setX(fromFixed(velocity_x));
    tp.velocity.setY(fromFixed(velocity_y));

    if (rawdata) {
        const int rawPosCount = rawdata->size / sizeof(float) / 2;
        float *p = static_cast<float *>(rawdata->data);
        for (int i = 0; i < rawPosCount; ++i) {
            float x = *p++;
            float y = *p++;
            tp.rawPositions.append(QPointF(x, y));
        }
    }

    mTouchPoints.append(tp);
    mTimestamp = time;

    if (!--mPointsLeft)
        sendTouchEvent();
}

void QWaylandTouchExtension::sendTouchEvent()
{
    // Copy all points, that are in the previous but not in the current list, as stationary.
    for (int i = 0; i < mPrevTouchPoints.count(); ++i) {
        const QWindowSystemInterface::TouchPoint &prevPoint(mPrevTouchPoints.at(i));
        if (prevPoint.state == Qt::TouchPointReleased)
            continue;
        bool found = false;
        for (int j = 0; j < mTouchPoints.count(); ++j)
            if (mTouchPoints.at(j).id == prevPoint.id) {
                found = true;
                break;
            }
        if (!found) {
            QWindowSystemInterface::TouchPoint p = prevPoint;
            p.state = Qt::TouchPointStationary;
            mTouchPoints.append(p);
        }
    }

    if (mTouchPoints.isEmpty()) {
        mPrevTouchPoints.clear();
        return;
    }

    QWindowSystemInterface::handleTouchEvent(mTargetWindow, mTimestamp, mTouchDevice, mTouchPoints);

    Qt::TouchPointStates states = 0;
    for (int i = 0; i < mTouchPoints.count(); ++i)
        states |= mTouchPoints.at(i).state;

    if (mFlags & QT_TOUCH_EXTENSION_FLAGS_MOUSE_FROM_TOUCH) {
        if (states == Qt::TouchPointPressed)
            mMouseSourceId = mTouchPoints.first().id;
        for (int i = 0; i < mTouchPoints.count(); ++i) {
            const QWindowSystemInterface::TouchPoint &tp(mTouchPoints.at(i));
            if (tp.id == mMouseSourceId) {
                Qt::MouseButtons buttons = tp.state == Qt::TouchPointReleased ? Qt::NoButton : Qt::LeftButton;
                mLastMouseGlobal = tp.area.center();
                QPoint globalPoint = mLastMouseGlobal.toPoint();
                QPointF delta = mLastMouseGlobal - globalPoint;
                mLastMouseLocal = mTargetWindow->mapFromGlobal(globalPoint) + delta;
                QWindowSystemInterface::handleMouseEvent(mTargetWindow, mTimestamp, mLastMouseLocal, mLastMouseGlobal, buttons);
                if (buttons == Qt::NoButton)
                    mMouseSourceId = -1;
                break;
            }
        }
    }

    mPrevTouchPoints = mTouchPoints;
    mTouchPoints.clear();

    if (states == Qt::TouchPointReleased)
        mPrevTouchPoints.clear();
}

void QWaylandTouchExtension::touchCanceled()
{
    mTouchPoints.clear();
    mPrevTouchPoints.clear();
    if (mMouseSourceId != -1)
        QWindowSystemInterface::handleMouseEvent(mTargetWindow, mTimestamp, mLastMouseLocal, mLastMouseGlobal, Qt::NoButton);
}

void QWaylandTouchExtension::touch_extension_configure(uint32_t flags)
{
    mFlags = flags;
}

}

QT_END_NAMESPACE
