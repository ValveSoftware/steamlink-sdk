/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the ActiveQt framework of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qaxutils_p.h"

#include <QtWidgets/QWidget>
#include <QtGui/QPixmap>
#include <QtGui/QRegion>
#include <QtGui/QWindow>
#include <QtGui/QGuiApplication>
#include <private/qhighdpiscaling_p.h>
#include <qpa/qplatformnativeinterface.h>
#include <qpa/qplatformpixmap.h>
#include <QtGui/private/qpixmap_raster_p.h>
#include <QtCore/QScopedArrayPointer>
#include <QtCore/QRect>

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

static inline QWindow *windowForWidget(QWidget *widget)
{
    if (QWindow *window = widget->windowHandle())
        return window;
    if (QWidget *nativeParent = widget->nativeParentWidget())
        return nativeParent->windowHandle();
    return 0;
}

HWND hwndForWidget(QWidget *widget)
{
    if (QWindow *window = windowForWidget(widget))
        return static_cast<HWND> (QGuiApplication::platformNativeInterface()->nativeResourceForWindow("handle", window));
    return 0;
}

// Code courtesy of QWindowsXPStyle
static void addRectToHrgn(HRGN &winRegion, const QRect &r)
{
    HRGN rgn = CreateRectRgn(r.left(), r.top(), r.x() + r.width(), r.y() + r.height());
    if (rgn) {
        HRGN dest = CreateRectRgn(0,0,0,0);
        int result = CombineRgn(dest, winRegion, rgn, RGN_OR);
        if (result) {
            DeleteObject(winRegion);
            winRegion = dest;
        }
        DeleteObject(rgn);
    }
}

HRGN qaxHrgnFromQRegion(QRegion region, const QWindow *window)
{
    region = QHighDpi::toNativeLocalRegion(region, window);
    HRGN hRegion = CreateRectRgn(0, 0, 0, 0);
    if (region.rectCount() == 1) {
        addRectToHrgn(hRegion, region.boundingRect());
        return hRegion;
    }
    foreach (const QRect &rect, region.rects())
        addRectToHrgn(hRegion, rect);
    return hRegion;
}

// HIMETRICS scaling
static const qreal himetricsPerInch = 2540;

static inline long qaxMapPixToLogHiMetrics(int x, qreal logicalDpi, qreal factor)
{
    return qRound((qreal(x) * himetricsPerInch * factor) / logicalDpi);
}

static inline int qaxMapLogHiMetricsToPix(long x, qreal logicalDpi, qreal factor)
{
    return qRound(logicalDpi * qreal(x) / (himetricsPerInch * factor));
}

SIZEL qaxMapPixToLogHiMetrics(const QSize &s, const QDpi &d, const QWindow *w)
{
    const qreal factor = QHighDpiScaling::factor(w);
    const SIZEL result = {
        qaxMapPixToLogHiMetrics(s.width(), d.first, factor),
        qaxMapPixToLogHiMetrics(s.height(), d.second, factor)
    };
    return result;
}

QSize qaxMapLogHiMetricsToPix(const SIZEL &s, const QDpi &d, const QWindow *w)
{
    const qreal factor = QHighDpiScaling::factor(w);
    const QSize result(qaxMapLogHiMetricsToPix(s.cx, d.first, factor),
                       qaxMapLogHiMetricsToPix(s.cy, d.second, factor));
    return result;
}

// Cache logical DPI in case High DPI scaling is active (which case
// the fake logical DPI it calculates is not suitable).

static QDpi cachedSystemLogicalDpi(-1, -1);

void qaxClearCachedSystemLogicalDpi() // Call from WM_DISPLAYCHANGE
{
    cachedSystemLogicalDpi = QDpi(-1, -1);
}

static inline QDpi systemLogicalDpi()
{
    if (cachedSystemLogicalDpi.first < 0) {
        const HDC displayDC = GetDC(0);
        cachedSystemLogicalDpi = QDpi(GetDeviceCaps(displayDC, LOGPIXELSX), GetDeviceCaps(displayDC, LOGPIXELSY));
        ReleaseDC(0, displayDC);
    }
    return cachedSystemLogicalDpi;
}

static inline QDpi paintDeviceLogicalDpi(const QPaintDevice *d)
{
    return QDpi(d->logicalDpiX(), d->logicalDpiY());
}

#ifdef QT_WIDGETS_LIB

SIZEL qaxMapPixToLogHiMetrics(const QSize &s, const QWidget *widget)
{
    return qaxMapPixToLogHiMetrics(s,
                                   QHighDpiScaling::isActive() ? systemLogicalDpi() : paintDeviceLogicalDpi(widget),
                                   widget->windowHandle());
}

QSize qaxMapLogHiMetricsToPix(const SIZEL &s, const QWidget *widget)
{
    return qaxMapLogHiMetricsToPix(s,
                                   QHighDpiScaling::isActive() ? systemLogicalDpi() : paintDeviceLogicalDpi(widget),
                                   widget->windowHandle());
}

QPoint qaxFromNativePosition(const QWidget *w, const QPoint &nativePos)
{
    const qreal factor = QHighDpiScaling::factor(w->windowHandle());
    return qFuzzyCompare(factor, 1)
        ? nativePos : (QPointF(nativePos) / factor).toPoint();
}

QPoint qaxNativeWidgetPosition(const QWidget *w)
{
    return qaxFromNativePosition(w, w->geometry().topLeft());
}

QSize qaxToNativeSize(const QWidget *w, const QSize &size)
{
    const qreal factor = QHighDpiScaling::factor(w->windowHandle());
    return qFuzzyCompare(factor, 1) ? size : (QSizeF(size) * factor).toSize();
}

QSize qaxNativeWidgetSize(const QWidget *w)
{
    return qaxToNativeSize(w, w->size());
}

QSize qaxFromNativeSize(const QWidget *w, const QSize &size)
{
    const qreal factor = QHighDpiScaling::factor(w->windowHandle());
    return qFuzzyCompare(factor, 1) ? size : (QSizeF(size) / factor).toSize();
}

RECT qaxNativeWidgetRect(const QWidget *w)
{
    return QHighDpiScaling::isActive()
        ? qaxQRect2Rect(QRect(qaxNativeWidgetPosition(w), qaxNativeWidgetSize(w)))
        : qaxQRect2Rect(w->geometry());
}

QRect qaxFromNativeRect(const RECT &r, const QWidget *w)
{
    const QRect qr = qaxRect2QRect(r);
    const qreal factor = QHighDpiScaling::factor(w->windowHandle());
    return qFuzzyCompare(factor, 1)
        ? qr
        : QRect((QPointF(qr.topLeft()) / factor).toPoint(), (QSizeF(qr.size()) / factor).toSize());
}

HRGN qaxHrgnFromQRegion(const QRegion &region, const QWidget *widget)
{
    return qaxHrgnFromQRegion(region, widget->windowHandle());
}

#endif // QT_WIDGETS_LIB

QByteArray qaxTypeInfoName(ITypeInfo *typeInfo, MEMBERID memId)
{
    QByteArray result;
    BSTR names;
    UINT cNames = 0;
    typeInfo->GetNames(memId, &names, 1, &cNames);
    if (cNames && names) {
        result = QString::fromWCharArray(names).toLatin1();
        SysFreeString(names);
    }
    return result;
}

QByteArrayList qaxTypeInfoNames(ITypeInfo *typeInfo, MEMBERID memId)
{
    QByteArrayList result;
    BSTR bstrNames[256];
    UINT maxNames = 255;
    UINT maxNamesOut = 0;
    typeInfo->GetNames(memId, reinterpret_cast<BSTR *>(&bstrNames), maxNames, &maxNamesOut);
    result.reserve(maxNamesOut);
    for (UINT p = 0; p < maxNamesOut; ++p) {
        result.append(QString::fromWCharArray(bstrNames[p]).toLatin1());
        SysFreeString(bstrNames[p]);
    }
    return result;
}

QT_END_NAMESPACE
