/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtMacExtras module of the Qt Toolkit.
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

#ifndef QMACFUNCTIONS_H
#define QMACFUNCTIONS_H

#if 0
#pragma qt_class(QtMac)
#endif

#include "QtMacExtras/qmacextrasglobal.h"

typedef struct CGImage *CGImageRef;
typedef struct CGContext *CGContextRef;

Q_FORWARD_DECLARE_OBJC_CLASS(NSData);
Q_FORWARD_DECLARE_OBJC_CLASS(NSImage);

QT_BEGIN_NAMESPACE

class QByteArray;
class QMenu;
class QMenuBar;
class QPixmap;
class QString;
class QUrl;
class QWindow;

namespace QtMac
{
#if QT_DEPRECATED_SINCE(5,3)
QT_DEPRECATED Q_MACEXTRAS_EXPORT NSData *toNSData(const QByteArray &data);
QT_DEPRECATED Q_MACEXTRAS_EXPORT QByteArray fromNSData(const NSData *data);
#endif

Q_MACEXTRAS_EXPORT CGImageRef toCGImageRef(const QPixmap &pixmap);
Q_MACEXTRAS_EXPORT QPixmap fromCGImageRef(CGImageRef image);

Q_MACEXTRAS_EXPORT CGContextRef currentCGContext();

#ifdef Q_OS_OSX
Q_MACEXTRAS_EXPORT void setBadgeLabelText(const QString &text);
Q_MACEXTRAS_EXPORT QString badgeLabelText();

Q_MACEXTRAS_EXPORT NSImage *toNSImage(const QPixmap &pixmap);

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
Q_MACEXTRAS_EXPORT bool isMainWindow(QWindow *window);
#endif
#endif // Q_OS_OSX

#ifdef Q_OS_IOS
Q_MACEXTRAS_EXPORT void setApplicationIconBadgeNumber(int number);
Q_MACEXTRAS_EXPORT int applicationIconBadgeNumber();
#endif // Q_OS_IOS
}

QT_END_NAMESPACE

#endif // QMACFUNCTIONS_H
