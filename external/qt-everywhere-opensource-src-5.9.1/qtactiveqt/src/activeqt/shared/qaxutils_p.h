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

#ifndef QAXUTILS_P_H
#define QAXUTILS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qt_windows.h>
#include <QtCore/QMetaType>
#include <QtCore/QPair>
#include <QtCore/QRect>

QT_BEGIN_NAMESPACE

class QWidget;
class QPixmap;
class QRect;
class QRegion;
class QWindow;

HWND hwndForWidget(QWidget *widget);
HRGN qaxHrgnFromQRegion(QRegion region, const QWindow *window);

typedef QPair<qreal, qreal> QDpi;

extern SIZEL qaxMapPixToLogHiMetrics(const QSize &s, const QDpi &d, const QWindow *w);
extern QSize qaxMapLogHiMetricsToPix(const SIZEL &s, const QDpi &d, const QWindow *w);

void qaxClearCachedSystemLogicalDpi(); // Call from WM_DISPLAYCHANGE

static inline RECT qaxQRect2Rect(const QRect &r)
{
    RECT result = { r.x(), r.y(), r.x() + r.width(), r.y() + r.height() };
    return result;
}

static inline QSize qaxSizeOfRect(const RECT &rect)
{
    return QSize(rect.right -rect.left, rect.bottom - rect.top);
}

static inline QRect qaxRect2QRect(const RECT &rect)
{
    return QRect(QPoint(rect.left, rect.top), qaxSizeOfRect(rect));
}

static inline RECT qaxContentRect(const QSize &size)  // Size with topleft = 0,0
{
    RECT result = { 0, 0, size.width(), size.height() };
    return result;
}

#ifdef QT_WIDGETS_LIB
SIZEL qaxMapPixToLogHiMetrics(const QSize &s, const QWidget *widget);
QSize qaxMapLogHiMetricsToPix(const SIZEL &s, const QWidget *widget);

QPoint qaxFromNativePosition(const QWidget *w, const QPoint &nativePos);
QPoint qaxNativeWidgetPosition(const QWidget *w);
QSize qaxToNativeSize(const QWidget *w, const QSize &size);
QSize qaxFromNativeSize(const QWidget *w, const QSize &size);
QSize qaxNativeWidgetSize(const QWidget *w);
RECT qaxNativeWidgetRect(const QWidget *w);
QRect qaxFromNativeRect(const RECT &r, const QWidget *w);
HRGN qaxHrgnFromQRegion(const QRegion &region, const QWidget *widget);
#endif // QT_WIDGETS_LIB

QByteArray qaxTypeInfoName(ITypeInfo *typeInfo, MEMBERID memId);
QByteArrayList qaxTypeInfoNames(ITypeInfo *typeInfo, MEMBERID memId);

QT_END_NAMESPACE

Q_DECLARE_METATYPE(IDispatch**)

#endif
